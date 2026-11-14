#include "STM8Assembler.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#include "ANTLRInputStream.h"
#include "BaseErrorListener.h"
#include "CommonTokenStream.h"
#include "RccAsmLexer.h"
#include "RccAsmParser.h"

namespace rcc {

namespace {

[[noreturn]] void fail(const std::string &msg) { throw std::runtime_error("STM8Assembler: " + msg); }

// --- Parsing, via the shared grammar/RccAsm.g4 ---------------------------

enum class SylKind { Number, Symbol, Group };
struct Syllable {
    char sign = 0;
    SylKind kind;
    std::string text;
    std::vector<Syllable> group;
};

struct Line {
    std::vector<std::string> labels;
    enum Kind { None, Org, Db, Dw, Instr } kind = None;
    std::vector<std::vector<Syllable>> exprList;  // Org (1) / Db (N) / Dw (N)
    std::string mnemonic;                          // Instr
    std::vector<std::vector<Syllable>> operands;   // Instr
};

std::string toLower(std::string s) {
    for (char &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::vector<Syllable> exprToSyllables(RccAsmParser::ExprContext *ctx) {
    std::vector<Syllable> result;
    for (auto *syl : ctx->syllable()) {
        char sign = 0;
        if (auto *s = syl->sign()) sign = s->PLUS() ? '+' : '-';
        auto *atom = syl->atom();
        if (atom->expr() != nullptr) {
            Syllable g;
            g.sign = sign;
            g.kind = SylKind::Group;
            g.group = exprToSyllables(atom->expr());
            result.push_back(std::move(g));
        } else if (atom->NUMBER()) {
            result.push_back({sign, SylKind::Number, atom->NUMBER()->getText(), {}});
        } else {
            result.push_back({sign, SylKind::Symbol, atom->IDENT()->getText(), {}});
        }
    }
    if (result.empty()) fail("empty expression");
    return result;
}

class ThrowingErrorListener : public antlr4::BaseErrorListener {
   public:
    void syntaxError(antlr4::Recognizer *, antlr4::Token *, size_t line, size_t col, const std::string &msg,
                      std::exception_ptr) override {
        fail(std::to_string(line) + ":" + std::to_string(col) + ": " + msg);
    }
};

std::vector<Syllable> buildOperand(RccAsmParser::OperandContext *ctx) {
    if (ctx->LBRACKET()) {
        fail("bracketed memory operands aren't part of this target's dialect - indirect-through-x addressing has "
             "its own dedicated mnemonics instead (ldxind/stxind)");
    }
    return exprToSyllables(ctx->expr());
}

Line buildLine(RccAsmParser::LineContext *ctx) {
    Line line;
    for (auto *lbl : ctx->label()) line.labels.push_back(lbl->IDENT()->getText());

    auto *stmt = ctx->statement();
    if (stmt == nullptr) {
        line.kind = Line::None;
        return line;
    }
    if (dynamic_cast<RccAsmParser::AssignStmtContext *>(stmt)) {
        fail("'='-style equates aren't part of this target's dialect - every STM8 pseudo-register here is a plain "
             "memory label (see STM8Codegen.h), never a fixed compile-time equate");
    }
    if (dynamic_cast<RccAsmParser::TimesStmtContext *>(stmt)) {
        fail("'times' isn't part of this target's dialect");
    }
    auto *instr = dynamic_cast<RccAsmParser::InstrStmtContext *>(stmt);
    if (!instr) fail("internal: unhandled statement alternative");

    std::string mnemonic = toLower(instr->mnemonic->getText());
    if (mnemonic == "org") {
        line.kind = Line::Org;
        if (instr->operand().empty() || instr->operand(0)->LBRACKET()) fail("'org' needs a plain address operand");
        line.exprList.push_back(exprToSyllables(instr->operand(0)->expr()));
        return line;
    }
    if (mnemonic == "db") {
        line.kind = Line::Db;
        for (auto *op : instr->operand()) line.exprList.push_back(buildOperand(op));
        return line;
    }
    // Unlike `db`, "dw"'s expression can resolve to any 16-bit value
    // (symbol address included), written out big-endian.
    if (mnemonic == "dw") {
        line.kind = Line::Dw;
        for (auto *op : instr->operand()) line.exprList.push_back(buildOperand(op));
        return line;
    }

    line.kind = Line::Instr;
    line.mnemonic = mnemonic;
    for (auto *op : instr->operand()) line.operands.push_back(buildOperand(op));
    return line;
}

std::vector<Line> parseProgram(const std::string &text) {
    antlr4::ANTLRInputStream input(text);
    RccAsmLexer lexer(&input);
    ThrowingErrorListener errors;
    lexer.removeErrorListeners();
    lexer.addErrorListener(&errors);

    antlr4::CommonTokenStream tokens(&lexer);
    RccAsmParser parser(&tokens);
    parser.removeErrorListeners();
    parser.addErrorListener(&errors);

    RccAsmParser::ProgramContext *prog = parser.program();

    std::vector<Line> lines;
    for (auto *lineCtx : prog->line()) lines.push_back(buildLine(lineCtx));
    return lines;
}

// A single-syllable, unsigned, bare-identifier operand - the shape a
// register name (a/x/y/sp) always takes. Returns nullopt if the operand
// isn't that shape, in which case the caller evaluates it as an ordinary expr.
std::optional<std::string> asBareIdent(const std::vector<Syllable> &operand) {
    if (operand.size() != 1) return std::nullopt;
    const Syllable &s = operand[0];
    if (s.sign != 0 || s.kind != SylKind::Symbol) return std::nullopt;
    return toLower(s.text);
}

// A pending fixup that becomes a real ObjRelocation once assembly is done -
// `symbolName` is either an unresolved reference or the reserved
// pseudo-symbol ".." for a same-file address's base-shift.
struct Relocation {
    uint32_t byteOffset;
    std::string symbolName;
};

// --- Two-pass assembly ----------------------------------------------------
//
// Opcode bytes are this backend's own invention, grouped by operand shape,
// not any real STM8 opcode map. Every 16-bit operand is encoded big-endian.
class Assembler {
   public:
    explicit Assembler(bool allowExternal) : allowExternal_(allowExternal) {}

    void run(const std::vector<Line> &lines) {
        stage_ = 1;
        pass(lines);
        stage_ = 2;
        pass(lines);
    }

    const std::vector<uint8_t> &bytes() const { return bytes_; }
    uint32_t orgBase() const { return orgBase_; }
    const std::unordered_map<std::string, uint32_t> &labels() const { return labels_; }
    const std::vector<Relocation> &relocations() const { return relocations_; }

   private:
    void pass(const std::vector<Line> &lines) {
        bytes_.clear();
        relocations_.clear();
        offset_ = 0;
        orgBase_ = 0;
        for (const auto &line : lines) processLine(line);
    }

    void processLine(const Line &line) {
        for (const auto &lbl : line.labels) labels_[lbl] = orgBase_ + offset_;
        switch (line.kind) {
            case Line::None:
                return;
            case Line::Org:
                orgBase_ = evalExpr(line.exprList.at(0));
                return;
            case Line::Db:
                for (const auto &e : line.exprList) emitByteOperand(e);
                return;
            case Line::Dw:
                for (const auto &e : line.exprList) emit16(e);
                return;
            case Line::Instr:
                encodeInstr(line.mnemonic, line.operands);
                return;
        }
    }

    // Once `allowExternal_` is set, any local address reference (not just
    // an external one) needs a relocation, since labels resolve to
    // "offset from org 0" until a linker adds the real base.
    uint32_t evalExpr(const std::vector<Syllable> &syls, std::string *externalName = nullptr,
                       bool *needsBaseShift = nullptr) {
        uint32_t total = 0;
        for (const auto &syl : syls) {
            bool wasExternal = false, wasLocalAddress = false;
            uint32_t v = resolveSyllable(syl, &wasExternal, &wasLocalAddress);
            if (wasExternal && externalName) *externalName = syl.text;
            if (wasLocalAddress && needsBaseShift) *needsBaseShift = true;
            if (syl.sign == '-') {
                total -= v;
            } else {
                total += v;
            }
        }
        return total;
    }

    uint32_t resolveSyllable(const Syllable &syl, bool *wasExternal = nullptr, bool *wasLocalAddress = nullptr) {
        if (syl.kind == SylKind::Group) return evalExpr(syl.group);
        if (syl.kind == SylKind::Number) {
            const std::string &t = syl.text;
            if (t.size() > 2 && t[0] == '0' && (t[1] == 'x' || t[1] == 'X')) {
                return static_cast<uint32_t>(std::stoul(t.substr(2), nullptr, 16));
            }
            return static_cast<uint32_t>(std::stoul(t));
        }
        auto it = labels_.find(syl.text);
        if (it != labels_.end()) {
            if (allowExternal_ && wasLocalAddress) *wasLocalAddress = true;
            return it->second;
        }
        if (stage_ == 2) {
            if (allowExternal_) {
                if (wasExternal) *wasExternal = true;
                return 0;
            }
            fail("undefined symbol '" + syl.text + "'");
        }
        return 0;
    }

    // --- byte emission --------------------------------------------

    void emit8(uint8_t b) {
        bytes_.push_back(b);
        offset_ += 1;
    }

    void emitByteOperand(const std::vector<Syllable> &e) {
        uint32_t v = evalExpr(e);
        if (stage_ == 2 && v > 0xFF) fail("'db' value doesn't fit in a byte");
        emit8(static_cast<uint8_t>(v & 0xFF));
    }

    void emitImm8(const std::vector<Syllable> &e) {
        uint32_t v = evalExpr(e);
        emit8(static_cast<uint8_t>(v & 0xFF));
    }

    // The one choke point every absolute address this dialect emits goes
    // through, so it's also the one place that records a relocation.
    void emit16(const std::vector<Syllable> &e) {
        std::string externalName;
        bool needsBaseShift = false;
        uint32_t v = evalExpr(e, &externalName, &needsBaseShift);
        if (stage_ == 2 && allowExternal_) {
            if (needsBaseShift) relocations_.push_back({offset_, ".."});
            if (!externalName.empty()) relocations_.push_back({offset_, externalName});
        }
        emit8(static_cast<uint8_t>((v >> 8) & 0xFF));
        emit8(static_cast<uint8_t>(v & 0xFF));
    }

    // --- operand helpers --------------------------------------------

    const std::vector<Syllable> &operandAt(const std::vector<std::vector<Syllable>> &ops, size_t i,
                                            const std::string &mnemonic, size_t wantCount) {
        if (ops.size() != wantCount) {
            fail("'" + mnemonic + "' needs exactly " + std::to_string(wantCount) + " operand(s)");
        }
        return ops[i];
    }

    // x=0, y=1.
    uint8_t wordRegCode(const std::vector<Syllable> &operand, const std::string &mnemonic) {
        auto name = asBareIdent(operand);
        if (!name) fail("'" + mnemonic + "' needs a register name (x/y), not an expression");
        if (*name == "x") return 0;
        if (*name == "y") return 1;
        fail("'" + mnemonic + "': '" + *name + "' isn't a word register (x/y)");
    }

    void requireIdent(const std::vector<Syllable> &operand, const std::string &want, const std::string &mnemonic) {
        auto name = asBareIdent(operand);
        if (!name || *name != want) fail("'" + mnemonic + "' needs '" + want + "' here");
    }

    void encodeInstr(const std::string &mnemonic, const std::vector<std::vector<Syllable>> &ops) {
        static const std::unordered_map<std::string, uint8_t> implied = {
            {"nop", 0x00},    {"halt", 0x01},   {"ret", 0x02},    {"rim", 0x03},    {"sim", 0x04},
            {"addw_xy", 0x05}, {"subw_xy", 0x06}, {"cpw_xy", 0x07}, {"exg_xy", 0x08},
            {"ldxind", 0x09}, {"stxind", 0x0A}, {"inca", 0x0B},   {"deca", 0x0C},   {"tnza", 0x0D},
            // scf/rcf: real STM8 set/clear carry, employed to seed a
            // chained rlc/rrc for a long/long long shift (see genWideShift()).
            {"scf", 0x0E}, {"rcf", 0x0F},
        };
        if (mnemonic == "addw" || mnemonic == "subw" || mnemonic == "cpw" || mnemonic == "exg") {
            requireIdent(operandAt(ops, 0, mnemonic, 2), "x", mnemonic);
            requireIdent(operandAt(ops, 1, mnemonic, 2), "y", mnemonic);
            emit8(implied.at(mnemonic + "_xy"));
            return;
        }
        if (mnemonic == "inc" || mnemonic == "dec" || mnemonic == "tnz") {
            // Accumulator-only forms (inc a/dec a/tnz a).
            requireIdent(operandAt(ops, 0, mnemonic, 1), "a", mnemonic);
            emit8(implied.at(mnemonic + "a"));
            return;
        }
        // Accumulator-only byte complement/rotate-through-carry/shift, the
        // byte-serial primitives genWideNegate()/genWideShift() chain through.
        static const std::unordered_map<std::string, uint8_t> aluA = {
            {"cpl", 0x68}, {"rlc", 0x6A}, {"rrc", 0x6C}, {"sla", 0x6E}, {"sra", 0x70}, {"srl", 0x72},
        };
        if (auto it = aluA.find(mnemonic); it != aluA.end()) {
            requireIdent(operandAt(ops, 0, mnemonic, 1), "a", mnemonic);
            emit8(it->second);
            return;
        }
        if (auto it = implied.find(mnemonic); it != implied.end() && ops.empty()) {
            emit8(it->second);
            return;
        }

        if (mnemonic == "push" || mnemonic == "pop") {
            uint8_t r = wordRegCode(operandAt(ops, 0, mnemonic, 1), mnemonic);
            emit8(static_cast<uint8_t>((mnemonic == "push" ? 0x10 : 0x12) | r));
            return;
        }
        if (mnemonic == "incw" || mnemonic == "decw" || mnemonic == "negw" || mnemonic == "cplw" ||
            mnemonic == "slaw" || mnemonic == "srlw" || mnemonic == "tnzw" || mnemonic == "sraw") {
            static const std::unordered_map<std::string, uint8_t> base = {
                {"incw", 0x14}, {"decw", 0x16}, {"negw", 0x18}, {"cplw", 0x1A}, {"slaw", 0x1C},
                {"srlw", 0x1E}, {"tnzw", 0x20},
                // sraw: word arithmetic (sign-preserving) right shift,
                // srlw's signed counterpart.
                {"sraw", 0x22},
            };
            uint8_t r = wordRegCode(operandAt(ops, 0, mnemonic, 1), mnemonic);
            emit8(static_cast<uint8_t>(base.at(mnemonic) | r));
            return;
        }
        if (mnemonic == "ldwi") {
            auto name = asBareIdent(operandAt(ops, 0, mnemonic, 2));
            uint8_t base;
            if (name && *name == "sp") {
                base = 0x30;
            } else {
                base = static_cast<uint8_t>(0x32 | wordRegCode(operandAt(ops, 0, mnemonic, 2), mnemonic));
            }
            emit8(base);
            emit16(operandAt(ops, 1, mnemonic, 2));
            return;
        }
        if (mnemonic == "ldw") {
            // Either "ldw <reg>, <mem>" or "ldw <mem>, <reg>"; which
            // operand is the bare register name picks the direction.
            auto dstReg = asBareIdent(operandAt(ops, 0, mnemonic, 2));
            if (dstReg && (*dstReg == "x" || *dstReg == "y")) {
                emit8(static_cast<uint8_t>(0x40 | (*dstReg == "x" ? 0 : 1)));
                emit16(operandAt(ops, 1, mnemonic, 2));
                return;
            }
            auto srcReg = asBareIdent(operandAt(ops, 1, mnemonic, 2));
            if (!srcReg || (*srcReg != "x" && *srcReg != "y")) {
                fail("'ldw' needs a word register (x/y) on one side");
            }
            emit8(static_cast<uint8_t>(0x42 | (*srcReg == "x" ? 0 : 1)));
            emit16(operandAt(ops, 0, mnemonic, 2));
            return;
        }
        if (mnemonic == "ld") {
            // Byte equivalent of ldw above, always through `a`.
            auto dstReg = asBareIdent(operandAt(ops, 0, mnemonic, 2));
            if (dstReg && *dstReg == "a") {
                emit8(0x44);
                emit16(operandAt(ops, 1, mnemonic, 2));
                return;
            }
            requireIdent(operandAt(ops, 1, mnemonic, 2), "a", mnemonic);
            emit8(0x45);
            emit16(operandAt(ops, 0, mnemonic, 2));
            return;
        }
        if (mnemonic == "ldi") {
            requireIdent(operandAt(ops, 0, mnemonic, 2), "a", mnemonic);
            emit8(0x46);
            emitImm8(operandAt(ops, 1, mnemonic, 2));
            return;
        }
        static const std::unordered_map<std::string, uint8_t> aluMem = {
            {"and", 0x50}, {"or", 0x52}, {"xor", 0x54},
        };
        if (auto it = aluMem.find(mnemonic); it != aluMem.end()) {
            requireIdent(operandAt(ops, 0, mnemonic, 2), "a", mnemonic);
            emit8(it->second);
            emit16(operandAt(ops, 1, mnemonic, 2));
            return;
        }
        static const std::unordered_map<std::string, uint8_t> aluImm = {
            {"andi", 0x56}, {"ori", 0x58}, {"xori", 0x5A},
        };
        if (auto it = aluImm.find(mnemonic); it != aluImm.end()) {
            requireIdent(operandAt(ops, 0, mnemonic, 2), "a", mnemonic);
            emit8(it->second);
            emitImm8(operandAt(ops, 1, mnemonic, 2));
            return;
        }
        // add/sub a, mem: byte add/subtract with no carry chained, the
        // least-significant-byte step of a multi-byte +/- (genWideAdd()).
        static const std::unordered_map<std::string, uint8_t> aluMemArith = {
            {"add", 0x74}, {"sub", 0x76},
        };
        if (auto it = aluMemArith.find(mnemonic); it != aluMemArith.end()) {
            requireIdent(operandAt(ops, 0, mnemonic, 2), "a", mnemonic);
            emit8(it->second);
            emit16(operandAt(ops, 1, mnemonic, 2));
            return;
        }
        // addi a, imm: used only to add 1 into a multi-byte negate's least
        // significant byte; every later byte folds the carry via adc a, zero.
        if (mnemonic == "addi") {
            requireIdent(operandAt(ops, 0, mnemonic, 2), "a", mnemonic);
            emit8(0x78);
            emitImm8(operandAt(ops, 1, mnemonic, 2));
            return;
        }
        // adc/sbc a, mem: add-with-carry/subtract-with-borrow, the
        // ripple-carry idiom a multi-byte +/- chains across bytes through.
        static const std::unordered_map<std::string, uint8_t> aluMemCarry = {
            {"adc", 0x5C}, {"sbc", 0x5E},
        };
        if (auto it = aluMemCarry.find(mnemonic); it != aluMemCarry.end()) {
            requireIdent(operandAt(ops, 0, mnemonic, 2), "a", mnemonic);
            emit8(it->second);
            emit16(operandAt(ops, 1, mnemonic, 2));
            return;
        }
        if (mnemonic == "call" || mnemonic == "jp") {
            emit8(mnemonic == "call" ? 0x60 : 0x61);
            emit16(operandAt(ops, 0, mnemonic, 1));
            return;
        }
        // Real STM8 `CALL [mem]` indirect addressing: unlike plain `call`,
        // the operand names the memory location holding the entry point,
        // not the entry point itself.
        if (mnemonic == "callind") {
            emit8(0x66);
            emit16(operandAt(ops, 0, mnemonic, 1));
            return;
        }
        static const std::unordered_map<std::string, uint8_t> condJump = {
            {"jreq", 0x62}, {"jrne", 0x63}, {"jrult", 0x64}, {"jruge", 0x65},
        };
        if (auto it = condJump.find(mnemonic); it != condJump.end()) {
            emit8(it->second);
            emit16(operandAt(ops, 0, mnemonic, 1));
            return;
        }

        fail("unsupported instruction '" + mnemonic + "'");
    }

    std::vector<uint8_t> bytes_;
    std::unordered_map<std::string, uint32_t> labels_;
    std::vector<Relocation> relocations_;
    uint32_t offset_ = 0;
    uint32_t orgBase_ = 0;
    int stage_ = 1;
    bool allowExternal_;
};

ObjectFile buildObjectFile(const Assembler &a) {
    ObjectFile obj;
    obj.machineType = kSTM8CoffMachine;
    const auto &bytesVec = a.bytes();
    obj.textBytes.assign(bytesVec.begin(), bytesVec.end());

    for (const auto &kv : a.labels()) {
        obj.symbols.push_back({kv.first, kv.second - a.orgBase(), /*isDefined=*/true});
    }
    std::sort(obj.symbols.begin(), obj.symbols.end(),
              [](const ObjSymbol &x, const ObjSymbol &y) { return x.name < y.name; });

    // Every relocation's target symbol needs a symbol-table entry too,
    // including the pseudo-symbol ".." (resolved specially by the linker,
    // but still needs an index for the object-format writer to point at).
    std::unordered_map<std::string, bool> known;
    for (const auto &s : obj.symbols) known[s.name] = true;
    for (const auto &r : a.relocations()) {
        if (!known.count(r.symbolName)) {
            obj.symbols.push_back({r.symbolName, 0, /*isDefined=*/false});
            known[r.symbolName] = true;
        }
        obj.relocations.push_back({r.byteOffset, r.symbolName, kSTM8RelocAbs16});
    }
    return obj;
}

}  // namespace

std::string assembleSTM8(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/false);
    assembler.run(lines);
    const auto &bytes = assembler.bytes();
    return std::string(bytes.begin(), bytes.end());
}

ObjectFile assembleSTM8Object(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/true);
    assembler.run(lines);
    return buildObjectFile(assembler);
}

}  // namespace rcc
