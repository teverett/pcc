#include "I8080Assembler.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#include "ANTLRInputStream.h"
#include "BaseErrorListener.h"
#include "CommonTokenStream.h"
#include "PccAsmLexer.h"
#include "PccAsmParser.h"

namespace pcc {

namespace {

[[noreturn]] void fail(const std::string &msg) { throw std::runtime_error("I8080Assembler: " + msg); }

// --- Parsing, via the one shared grammar/PccAsm.g4 -----------------------
// Same shape as ../m6502/M6502Assembler.cpp's own Syllable/Line - see
// that file's own comment.

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

std::vector<Syllable> exprToSyllables(PccAsmParser::ExprContext *ctx) {
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

std::vector<Syllable> buildOperand(PccAsmParser::OperandContext *ctx) {
    if (ctx->LBRACKET()) fail("bracketed memory operands aren't part of this target's dialect - 8080 memory access "
                               "always goes through a named register (m/b/d), never a bracketed symbol");
    return exprToSyllables(ctx->expr());
}

Line buildLine(PccAsmParser::LineContext *ctx) {
    Line line;
    for (auto *lbl : ctx->label()) line.labels.push_back(lbl->IDENT()->getText());

    auto *stmt = ctx->statement();
    if (stmt == nullptr) {
        line.kind = Line::None;
        return line;
    }
    if (dynamic_cast<PccAsmParser::AssignStmtContext *>(stmt)) {
        fail("'='-style equates aren't part of this target's dialect - every 8080 pseudo-register here is a plain "
             "memory label (see I8080Codegen.h), never a fixed compile-time equate");
    }
    if (dynamic_cast<PccAsmParser::TimesStmtContext *>(stmt)) {
        fail("'times' isn't part of this target's dialect");
    }
    auto *instr = dynamic_cast<PccAsmParser::InstrStmtContext *>(stmt);
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
    // `dw` - a full little-endian 16-bit data word, resolved exactly
    // like `lxi`/`lhld`/`shld`'s own 16-bit operand (I8080Codegen.cpp's
    // own `arrayElemText()`/`declareArray()` uses this for a dispatch-
    // table array element that names a function/global directly: a
    // symbol's own address is only known to the assembler, once every
    // label is bound, never to the codegen that writes this directive
    // out).
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
    PccAsmLexer lexer(&input);
    ThrowingErrorListener errors;
    lexer.removeErrorListeners();
    lexer.addErrorListener(&errors);

    antlr4::CommonTokenStream tokens(&lexer);
    PccAsmParser parser(&tokens);
    parser.removeErrorListeners();
    parser.addErrorListener(&errors);

    PccAsmParser::ProgramContext *prog = parser.program();

    std::vector<Line> lines;
    for (auto *lineCtx : prog->line()) lines.push_back(buildLine(lineCtx));
    return lines;
}

// A single-syllable, unsigned, bare-identifier operand - the shape
// every register name (`a`, `b`, ..., `psw`) always takes. Returns
// its lowercased text, or nullopt if the operand isn't that shape at
// all (a real symbol/number/signed-or-grouped expression instead),
// in which case the caller evaluates it as an ordinary expr.
std::optional<std::string> asBareIdent(const std::vector<Syllable> &operand) {
    if (operand.size() != 1) return std::nullopt;
    const Syllable &s = operand[0];
    if (s.sign != 0 || s.kind != SylKind::Symbol) return std::nullopt;
    return toLower(s.text);
}

// One pending fixup `assembleI8080Object()` (see I8080Assembler.h's
// own header comment) turns into a real ObjRelocation once assembly
// is done - `byteOffset` into `bytes()`, `symbolName` the reference
// that couldn't be resolved locally (or the reserved pseudo-symbol
// ".." for a same-file address's own needed base-shift).
struct Relocation {
    uint32_t byteOffset;
    std::string symbolName;
};

// --- Two-pass assembly ----------------------------------------------------
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

    // `externalName`/`needsBaseShift` mirror
    // ../z80/Z80Assembler.cpp's own `evalExpr` exactly - see
    // I8080Assembler.h's own header comment on why *any* local
    // address reference, not just an external one, needs a
    // relocation here once `allowExternal_` is set.
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

    // imm8 - any instruction's own immediate-byte operand.
    void emitImm8(const std::vector<Syllable> &e) {
        uint32_t v = evalExpr(e);
        emit8(static_cast<uint8_t>(v & 0xFF));
    }

    // addr16/imm16, and `dw`'s own data word - little-endian, full
    // 16-bit resolved value, no range check (any symbol in this
    // program's own 64KB address space is valid here).
    void emit16(const std::vector<Syllable> &e) {
        std::string externalName;
        bool needsBaseShift = false;
        uint32_t v = evalExpr(e, &externalName, &needsBaseShift);
        if (stage_ == 2 && allowExternal_) {
            if (needsBaseShift) relocations_.push_back({offset_, ".."});
            if (!externalName.empty()) relocations_.push_back({offset_, externalName});
        }
        emit8(static_cast<uint8_t>(v & 0xFF));
        emit8(static_cast<uint8_t>((v >> 8) & 0xFF));
    }

    // --- operand helpers --------------------------------------------

    const std::vector<Syllable> &operandAt(const std::vector<std::vector<Syllable>> &ops, size_t i,
                                            const std::string &mnemonic, size_t wantCount) {
        if (ops.size() != wantCount) {
            fail("'" + mnemonic + "' needs exactly " + std::to_string(wantCount) + " operand(s)");
        }
        return ops[i];
    }

    // The 8080's own r/M register field encoding, shared by MOV/MVI/
    // INR/DCR/ADD-family/CMP: b=0,c=1,d=2,e=3,h=4,l=5,m=6(memory via
    // HL),a=7.
    uint8_t reg8Code(const std::vector<Syllable> &operand, const std::string &mnemonic) {
        auto name = asBareIdent(operand);
        if (!name) fail("'" + mnemonic + "' needs a register name (a/b/c/d/e/h/l/m), not an expression");
        static const std::unordered_map<std::string, uint8_t> table = {
            {"b", 0}, {"c", 1}, {"d", 2}, {"e", 3}, {"h", 4}, {"l", 5}, {"m", 6}, {"a", 7},
        };
        auto it = table.find(*name);
        if (it == table.end()) fail("'" + mnemonic + "': '" + *name + "' isn't a register (a/b/c/d/e/h/l/m)");
        return it->second;
    }

    // LXI/DAD/INX/DCX's own register-pair field: b=0 (BC),d=1
    // (DE),h=2 (HL),sp=3.
    uint8_t pairCodeBDHSP(const std::vector<Syllable> &operand, const std::string &mnemonic) {
        auto name = asBareIdent(operand);
        if (!name) fail("'" + mnemonic + "' needs a register pair (b/d/h/sp), not an expression");
        static const std::unordered_map<std::string, uint8_t> table = {{"b", 0}, {"d", 1}, {"h", 2}, {"sp", 3}};
        auto it = table.find(*name);
        if (it == table.end()) fail("'" + mnemonic + "': '" + *name + "' isn't a register pair (b/d/h/sp)");
        return it->second;
    }

    // PUSH/POP's own register-pair field: b=0,d=1,h=2,psw=3 (the
    // accumulator+flags pair - not a real register pair anywhere
    // else).
    uint8_t pairCodeBDHPSW(const std::vector<Syllable> &operand, const std::string &mnemonic) {
        auto name = asBareIdent(operand);
        if (!name) fail("'" + mnemonic + "' needs a register pair (b/d/h/psw), not an expression");
        static const std::unordered_map<std::string, uint8_t> table = {{"b", 0}, {"d", 1}, {"h", 2}, {"psw", 3}};
        auto it = table.find(*name);
        if (it == table.end()) fail("'" + mnemonic + "': '" + *name + "' isn't a register pair (b/d/h/psw)");
        return it->second;
    }

    // LDAX/STAX's own register-pair field: b=0 (BC),d=1 (DE) - the
    // only two pairs the 8080 can ever address 8-bit memory through
    // besides HL's own M-addressing.
    uint8_t pairCodeBD(const std::vector<Syllable> &operand, const std::string &mnemonic) {
        auto name = asBareIdent(operand);
        if (!name) fail("'" + mnemonic + "' needs a register pair (b/d), not an expression");
        static const std::unordered_map<std::string, uint8_t> table = {{"b", 0}, {"d", 1}};
        auto it = table.find(*name);
        if (it == table.end()) fail("'" + mnemonic + "': '" + *name + "' isn't a register pair (b/d)");
        return it->second;
    }

    // The full instruction vocabulary ../I8080Codegen.cpp ever emits,
    // encoded as real, standard Intel 8080 opcodes (see
    // I8080Assembler.h's own header comment) - not every 8080
    // instruction exists here, only the ones that backend needs.
    void encodeInstr(const std::string &mnemonic, const std::vector<std::vector<Syllable>> &ops) {
        static const std::unordered_map<std::string, uint8_t> implied = {
            {"nop", 0x00}, {"rlc", 0x07}, {"rrc", 0x0F}, {"ral", 0x17}, {"rar", 0x1F}, {"cma", 0x2F},
            {"stc", 0x37}, {"cmc", 0x3F}, {"xchg", 0xEB}, {"ret", 0xC9}, {"ei", 0xFB},  {"di", 0xF3},
            {"hlt", 0x76}, {"pchl", 0xE9},
        };
        if (auto it = implied.find(mnemonic); it != implied.end()) {
            if (!ops.empty()) fail("'" + mnemonic + "' takes no operand");
            emit8(it->second);
            return;
        }

        if (mnemonic == "lxi") {
            uint8_t rp = pairCodeBDHSP(operandAt(ops, 0, mnemonic, 2), mnemonic);
            emit8(static_cast<uint8_t>(0x01 | (rp << 4)));
            emit16(operandAt(ops, 1, mnemonic, 2));
            return;
        }
        if (mnemonic == "dad" || mnemonic == "inx" || mnemonic == "dcx") {
            uint8_t rp = pairCodeBDHSP(operandAt(ops, 0, mnemonic, 1), mnemonic);
            uint8_t base = mnemonic == "dad" ? 0x09 : (mnemonic == "inx" ? 0x03 : 0x0B);
            emit8(static_cast<uint8_t>(base | (rp << 4)));
            return;
        }
        if (mnemonic == "push" || mnemonic == "pop") {
            uint8_t rp = pairCodeBDHPSW(operandAt(ops, 0, mnemonic, 1), mnemonic);
            uint8_t base = mnemonic == "push" ? 0xC5 : 0xC1;
            emit8(static_cast<uint8_t>(base | (rp << 4)));
            return;
        }
        if (mnemonic == "ldax" || mnemonic == "stax") {
            uint8_t rp = pairCodeBD(operandAt(ops, 0, mnemonic, 1), mnemonic);
            uint8_t base = mnemonic == "ldax" ? 0x0A : 0x02;
            emit8(static_cast<uint8_t>(base | (rp << 4)));
            return;
        }

        static const std::unordered_map<std::string, uint8_t> addrOnly = {
            {"lda", 0x3A}, {"sta", 0x32}, {"lhld", 0x2A}, {"shld", 0x22}, {"jmp", 0xC3}, {"call", 0xCD},
        };
        if (auto it = addrOnly.find(mnemonic); it != addrOnly.end()) {
            emit8(it->second);
            emit16(operandAt(ops, 0, mnemonic, 1));
            return;
        }
        static const std::unordered_map<std::string, uint8_t> condJump = {
            {"jnz", 0xC2}, {"jz", 0xCA}, {"jnc", 0xD2}, {"jc", 0xDA},
            {"jpo", 0xE2}, {"jpe", 0xEA}, {"jp", 0xF2}, {"jm", 0xFA},
        };
        if (auto it = condJump.find(mnemonic); it != condJump.end()) {
            emit8(it->second);
            emit16(operandAt(ops, 0, mnemonic, 1));
            return;
        }

        if (mnemonic == "mov") {
            uint8_t dst = reg8Code(operandAt(ops, 0, mnemonic, 2), mnemonic);
            uint8_t src = reg8Code(operandAt(ops, 1, mnemonic, 2), mnemonic);
            if (dst == 6 && src == 6) fail("'mov m,m' isn't valid - that opcode (0x76) is HLT, not a move");
            emit8(static_cast<uint8_t>(0x40 | (dst << 3) | src));
            return;
        }
        if (mnemonic == "mvi") {
            uint8_t dst = reg8Code(operandAt(ops, 0, mnemonic, 2), mnemonic);
            emit8(static_cast<uint8_t>(0x06 | (dst << 3)));
            emitImm8(operandAt(ops, 1, mnemonic, 2));
            return;
        }
        if (mnemonic == "inr" || mnemonic == "dcr") {
            uint8_t r = reg8Code(operandAt(ops, 0, mnemonic, 1), mnemonic);
            emit8(static_cast<uint8_t>((mnemonic == "inr" ? 0x04 : 0x05) | (r << 3)));
            return;
        }
        static const std::unordered_map<std::string, uint8_t> aluReg = {
            {"add", 0x80}, {"adc", 0x88}, {"sub", 0x90}, {"sbb", 0x98},
            {"ana", 0xA0}, {"xra", 0xA8}, {"ora", 0xB0}, {"cmp", 0xB8},
        };
        if (auto it = aluReg.find(mnemonic); it != aluReg.end()) {
            uint8_t r = reg8Code(operandAt(ops, 0, mnemonic, 1), mnemonic);
            emit8(static_cast<uint8_t>(it->second | r));
            return;
        }
        static const std::unordered_map<std::string, uint8_t> aluImm = {
            {"adi", 0xC6}, {"aci", 0xCE}, {"sui", 0xD6}, {"sbi", 0xDE},
            {"ani", 0xE6}, {"xri", 0xEE}, {"ori", 0xF6}, {"cpi", 0xFE},
        };
        if (auto it = aluImm.find(mnemonic); it != aluImm.end()) {
            emit8(it->second);
            emitImm8(operandAt(ops, 0, mnemonic, 1));
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

// Builds the format-neutral ObjectFile a completed object-mode
// Assembler produced - see I8080Assembler.h's own `assembleI8080Object`
// comment, and ../z80/Z80Assembler.cpp's own `buildObjectFile` for the
// original version this is ported from (identical algorithm).
ObjectFile buildObjectFile(const Assembler &a) {
    ObjectFile obj;
    obj.machineType = kI8080CoffMachine;
    const auto &bytesVec = a.bytes();
    obj.textBytes.assign(bytesVec.begin(), bytesVec.end());

    for (const auto &kv : a.labels()) {
        obj.symbols.push_back({kv.first, kv.second - a.orgBase(), /*isDefined=*/true});
    }
    std::sort(obj.symbols.begin(), obj.symbols.end(),
              [](const ObjSymbol &x, const ObjSymbol &y) { return x.name < y.name; });

    std::unordered_map<std::string, bool> known;
    for (const auto &s : obj.symbols) known[s.name] = true;
    for (const auto &r : a.relocations()) {
        if (!known.count(r.symbolName)) {
            obj.symbols.push_back({r.symbolName, 0, /*isDefined=*/false});
            known[r.symbolName] = true;
        }
        obj.relocations.push_back({r.byteOffset, r.symbolName, kI8080RelocAbs16});
    }
    return obj;
}

}  // namespace

std::string assembleI8080(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/false);
    assembler.run(lines);
    const auto &bytes = assembler.bytes();
    return std::string(bytes.begin(), bytes.end());
}

ObjectFile assembleI8080Object(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/true);
    assembler.run(lines);
    return buildObjectFile(assembler);
}

}  // namespace pcc
