#include "I8008Assembler.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ANTLRInputStream.h"
#include "BaseErrorListener.h"
#include "CommonTokenStream.h"
#include "PccAsmLexer.h"
#include "PccAsmParser.h"

namespace pcc {

namespace {

[[noreturn]] void fail(const std::string &msg) { throw std::runtime_error("I8008Assembler: " + msg); }

// --- Parsing, via the one shared grammar/PccAsm.g4 -----------------------
// Same Syllable/Line shape as ../i8080/I8080Assembler.cpp's own - see
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
    enum Kind { None, Equate, Org, Db, Dw, Instr } kind = None;
    std::string equateName;                        // Equate
    std::vector<std::vector<Syllable>> exprList;    // Equate (1)/Org (1)/Db (N)
    std::string mnemonic;                            // Instr
    std::vector<std::vector<Syllable>> operands;      // Instr
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
    if (ctx->LBRACKET()) fail("bracketed memory operands aren't part of this target's dialect - 8008 memory access "
                               "always goes through H,L (register m), never a bracketed symbol");
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
    if (auto *assign = dynamic_cast<PccAsmParser::AssignStmtContext *>(stmt)) {
        line.kind = Line::Equate;
        line.equateName = assign->lhs->getText();
        line.exprList.push_back(exprToSyllables(assign->rhs));
        return line;
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
    // "dw" is a full, real 16-bit value (same shape as a `jmp`/`cal`
    // operand - see emit16()'s own comment) written out low byte then
    // high byte - unlike `db`, its own expression can resolve to any
    // 16-bit value, symbol address included, not just one that fits
    // in a single byte.
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

std::optional<std::string> asBareIdent(const std::vector<Syllable> &operand) {
    if (operand.size() != 1) return std::nullopt;
    const Syllable &s = operand[0];
    if (s.sign != 0 || s.kind != SylKind::Symbol) return std::nullopt;
    return toLower(s.text);
}

// One pending fixup `assembleI8008Object()` (see I8008Assembler.h's
// own header comment) turns into a real ObjRelocation once assembly
// is done - mirrors ../m6502/M6502Assembler.cpp's own `Relocation`
// (a `kind` field, not just one implied type, since this dialect also
// has three relocation kinds).
struct Relocation {
    uint32_t byteOffset;
    std::string symbolName;
    uint16_t kind;
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
        equateNames_.clear();
        offset_ = 0;
        orgBase_ = 0;
        for (const auto &line : lines) processLine(line);
    }

    void processLine(const Line &line) {
        for (const auto &lbl : line.labels) labels_[lbl] = orgBase_ + offset_;
        switch (line.kind) {
            case Line::None:
                return;
            case Line::Equate:
                labels_[line.equateName] = evalExpr(line.exprList.at(0));
                // A compile-time constant, the same in every object
                // regardless of where it's linked (this backend's own
                // fixed `ac`/`bc`/.../`sp` cells - see
                // I8008Codegen.h's own header comment) - never treated
                // as needing a `..` base-shift relocation, see
                // resolveSyllable()'s own check below. Mirrors
                // ../m6502/M6502Assembler.cpp's own `equateNames_`.
                equateNames_.insert(line.equateName);
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
    // I8008Assembler.h's own header comment on why *any* local
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
            if (allowExternal_ && wasLocalAddress && !equateNames_.count(syl.text)) *wasLocalAddress = true;
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

    // `mvi`'s own immediate operand - the LOW byte of whatever `e`
    // resolves to (a real assembler's `#<label` low-byte operator,
    // spelled as a plain mnemonic here - see I8008Codegen.h's own
    // header comment on pointAt()). Also the one place a *named*
    // cell's address (including an external global's own, via
    // pointAt()) needs a relocation - an ordinary numeric immediate
    // (e.g. `mvi a, 5`) never touches a symbol at all, so
    // `externalName`/`needsBaseShift` naturally stay empty/false for
    // that case with no special-casing needed (see I8008Assembler.h's
    // own header comment).
    void emitImmLow(const std::vector<Syllable> &e) {
        std::string externalName;
        bool needsBaseShift = false;
        uint32_t v = evalExpr(e, &externalName, &needsBaseShift);
        if (stage_ == 2 && allowExternal_) {
            if (needsBaseShift) relocations_.push_back({offset_, "..", kI8008RelocAbs16Lo});
            if (!externalName.empty()) relocations_.push_back({offset_, externalName, kI8008RelocAbs16Lo});
        }
        emit8(static_cast<uint8_t>(v & 0xFF));
    }
    // `mvihi`'s own immediate operand - the HIGH byte.
    void emitImmHigh(const std::vector<Syllable> &e) {
        std::string externalName;
        bool needsBaseShift = false;
        uint32_t v = evalExpr(e, &externalName, &needsBaseShift);
        if (stage_ == 2 && allowExternal_) {
            if (needsBaseShift) relocations_.push_back({offset_, "..", kI8008RelocAbs16Hi});
            if (!externalName.empty()) relocations_.push_back({offset_, externalName, kI8008RelocAbs16Hi});
        }
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

    // The 8008's own r/M register field - a=0,b=1,c=2,d=3,e=4,h=5,
    // l=6,m=7 (memory via HL) - *not* the 8080/Z80's own numbering
    // (see I8008Codegen.h's own header comment).
    uint8_t regCode(const std::vector<Syllable> &operand, const std::string &mnemonic) {
        auto name = asBareIdent(operand);
        if (!name) fail("'" + mnemonic + "' needs a register name (a/b/c/d/e/h/l/m), not an expression");
        static const std::unordered_map<std::string, uint8_t> table = {
            {"a", 0}, {"b", 1}, {"c", 2}, {"d", 3}, {"e", 4}, {"h", 5}, {"l", 6}, {"m", 7},
        };
        auto it = table.find(*name);
        if (it == table.end()) fail("'" + mnemonic + "': '" + *name + "' isn't a register (a/b/c/d/e/h/l/m)");
        return it->second;
    }

    // The full instruction vocabulary ../I8008Codegen.cpp ever emits,
    // encoded as real, standard Intel 8008 opcodes (see
    // I8008Assembler.h's own header comment) - not every 8008
    // instruction exists here (no RST, no INP/OUT, no conditional
    // RET), only the ones that backend needs.
    void encodeInstr(const std::string &mnemonic, const std::vector<std::vector<Syllable>> &ops) {
        static const std::unordered_map<std::string, uint8_t> implied = {
            {"rlc", 0x02}, {"rrc", 0x0A}, {"ral", 0x12}, {"rar", 0x1A}, {"ret", 0x07},
        };
        if (auto it = implied.find(mnemonic); it != implied.end()) {
            if (!ops.empty()) fail("'" + mnemonic + "' takes no operand");
            emit8(it->second);
            return;
        }
        static const std::unordered_map<std::string, uint8_t> condJump = {
            {"jfc", 0x40}, {"jfz", 0x48}, {"jfs", 0x50}, {"jfp", 0x58},
            {"jtc", 0x60}, {"jtz", 0x68}, {"jts", 0x70}, {"jtp", 0x78},
        };
        if (auto it = condJump.find(mnemonic); it != condJump.end()) {
            emit8(it->second);
            emit16(operandAt(ops, 0, mnemonic, 1));
            return;
        }
        static const std::unordered_map<std::string, uint8_t> condCall = {
            {"cfc", 0x42}, {"cfz", 0x4A}, {"cfs", 0x52}, {"cfp", 0x5A},
            {"ctc", 0x62}, {"ctz", 0x6A}, {"cts", 0x72}, {"ctp", 0x7A},
        };
        if (auto it = condCall.find(mnemonic); it != condCall.end()) {
            emit8(it->second);
            emit16(operandAt(ops, 0, mnemonic, 1));
            return;
        }
        if (mnemonic == "jmp") {
            emit8(0x44);
            emit16(operandAt(ops, 0, mnemonic, 1));
            return;
        }
        if (mnemonic == "cal") {
            emit8(0x46);
            emit16(operandAt(ops, 0, mnemonic, 1));
            return;
        }

        if (mnemonic == "mov") {
            uint8_t dst = regCode(operandAt(ops, 0, mnemonic, 2), mnemonic);
            uint8_t src = regCode(operandAt(ops, 1, mnemonic, 2), mnemonic);
            if (dst == 7 && src == 7) fail("'mov m,m' isn't valid - that opcode (0xFF) is HLT, not a move");
            emit8(static_cast<uint8_t>(0xC0 | (dst << 3) | src));
            return;
        }
        if (mnemonic == "mvi") {
            uint8_t dst = regCode(operandAt(ops, 0, mnemonic, 2), mnemonic);
            emit8(static_cast<uint8_t>(0x06 | (dst << 3)));
            emitImmLow(operandAt(ops, 1, mnemonic, 2));
            return;
        }
        if (mnemonic == "mvihi") {
            uint8_t dst = regCode(operandAt(ops, 0, mnemonic, 2), mnemonic);
            emit8(static_cast<uint8_t>(0x06 | (dst << 3)));
            emitImmHigh(operandAt(ops, 1, mnemonic, 2));
            return;
        }
        if (mnemonic == "inr" || mnemonic == "dcr") {
            uint8_t r = regCode(operandAt(ops, 0, mnemonic, 1), mnemonic);
            if (r == 0 || r == 7) {
                fail("'" + mnemonic + "' can't target 'a' or 'm' - the 8008 has no INR/DCR for either (see "
                                      "I8008Codegen.h's own header comment)");
            }
            emit8(static_cast<uint8_t>((r << 3) | (mnemonic == "inr" ? 0 : 1)));
            return;
        }
        static const std::unordered_map<std::string, uint8_t> aluReg = {
            {"add", 0x80}, {"adc", 0x88}, {"sub", 0x90}, {"sbb", 0x98},
            {"and", 0xA0}, {"xor", 0xA8}, {"or", 0xB0},  {"cmp", 0xB8},
        };
        if (auto it = aluReg.find(mnemonic); it != aluReg.end()) {
            uint8_t r = regCode(operandAt(ops, 0, mnemonic, 1), mnemonic);
            emit8(static_cast<uint8_t>(it->second | r));
            return;
        }
        static const std::unordered_map<std::string, uint8_t> aluImm = {
            {"adi", 0x04}, {"aci", 0x0C}, {"sui", 0x14}, {"sbi", 0x1C},
            {"ani", 0x24}, {"xri", 0x2C}, {"ori", 0x34}, {"cpi", 0x3C},
        };
        if (auto it = aluImm.find(mnemonic); it != aluImm.end()) {
            emit8(it->second);
            emitImmLow(operandAt(ops, 0, mnemonic, 1));
            return;
        }

        fail("unsupported instruction '" + mnemonic + "'");
    }

    // addr16 - little-endian, full 16-bit resolved value (every
    // JMP/CAL and conditional variant here is a plain absolute
    // 3-byte instruction - see I8008Codegen.h's own header comment:
    // no branch-range workaround needed anywhere in this backend).
    void emit16(const std::vector<Syllable> &e) {
        std::string externalName;
        bool needsBaseShift = false;
        uint32_t v = evalExpr(e, &externalName, &needsBaseShift);
        if (stage_ == 2 && allowExternal_) {
            if (needsBaseShift) relocations_.push_back({offset_, "..", kI8008RelocAbs16});
            if (!externalName.empty()) relocations_.push_back({offset_, externalName, kI8008RelocAbs16});
        }
        emit8(static_cast<uint8_t>(v & 0xFF));
        emit8(static_cast<uint8_t>((v >> 8) & 0xFF));
    }

    std::unordered_set<std::string> equateNames_;
    std::vector<Relocation> relocations_;
    bool allowExternal_;
    std::vector<uint8_t> bytes_;
    std::unordered_map<std::string, uint32_t> labels_;
    uint32_t offset_ = 0;
    uint32_t orgBase_ = 0;
    int stage_ = 1;
};

// Builds the format-neutral ObjectFile a completed object-mode
// Assembler produced - see ../m6502/M6502Assembler.cpp's own
// `buildObjectFile` for the template this is ported from (identical
// algorithm).
ObjectFile buildObjectFile(const Assembler &a) {
    ObjectFile obj;
    obj.machineType = kI8008CoffMachine;
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
        obj.relocations.push_back({r.byteOffset, r.symbolName, r.kind});
    }
    return obj;
}

}  // namespace

std::string assembleI8008(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/false);
    assembler.run(lines);
    const auto &bytes = assembler.bytes();
    return std::string(bytes.begin(), bytes.end());
}

ObjectFile assembleI8008Object(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/true);
    assembler.run(lines);
    return buildObjectFile(assembler);
}

}  // namespace pcc
