#include "AlphaAssembler.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
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

[[noreturn]] void fail(const std::string &msg) { throw std::runtime_error("AlphaAssembler: " + msg); }

// --- Parsing, via the shared grammar/RccAsm.g4 ---------------------------
enum class SylKind { Number, Symbol, Group };
struct Syllable {
    char sign = 0;
    SylKind kind;
    std::string text;
    std::vector<Syllable> group;
};

// A bare register is Reg; a parenthesized bare register (`(rb)`, this dialect's spelling of
// `jsr`/`jmp`/`ret`'s target-address operand) is RegDeferred; a bare signed number is Imm;
// anything else (a label, or `label+N`) is Mem, resolved as an address.
struct Operand {
    enum Kind { Reg, RegDeferred, Imm, Mem } kind = Imm;
    int reg = -1;
    std::vector<Syllable> expr;  // Imm/Mem
};

struct Line {
    std::vector<std::string> labels;
    enum Kind { None, Equate, Org, Db, Dh, Dw, Instr } kind = None;
    std::string equateName;
    std::vector<std::vector<Syllable>> exprList;  // Equate (1)/Org (1)/Db/Dh/Dw (N)
    std::string mnemonic;                           // Instr
    std::vector<Operand> operands;                  // Instr
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

// Matches the real Alpha calling-standard register names (Compaq/DEC's own convention, not
// an invented one): v0/t0-t7/s0-s5/fp/a0-a5/t8-t11/ra/t12(pv)/at/gp/sp/zero, register numbers
// 0-31 in that order.
int regNumber(const std::string &lower) {
    static const std::unordered_map<std::string, int> names = {
        {"v0", 0},  {"t0", 1},  {"t1", 2},  {"t2", 3},  {"t3", 4},  {"t4", 5},  {"t5", 6},  {"t6", 7},
        {"t7", 8},  {"s0", 9},  {"s1", 10}, {"s2", 11}, {"s3", 12}, {"s4", 13}, {"s5", 14}, {"fp", 15},
        {"a0", 16}, {"a1", 17}, {"a2", 18}, {"a3", 19}, {"a4", 20}, {"a5", 21}, {"t8", 22}, {"t9", 23},
        {"t10", 24}, {"t11", 25}, {"ra", 26}, {"t12", 27}, {"pv", 27}, {"at", 28}, {"gp", 29}, {"sp", 30},
        {"zero", 31},
    };
    auto it = names.find(lower);
    return it == names.end() ? -1 : it->second;
}

// A syllable that is exactly one unsigned register name.
int bareReg(const Syllable &s) {
    if (s.sign != 0 || s.kind != SylKind::Symbol) return -1;
    return regNumber(toLower(s.text));
}

// A parenthesized register, `(rb)`, ignoring the group's own sign.
int groupReg(const Syllable &s) {
    if (s.kind != SylKind::Group || s.group.size() != 1) return -1;
    return bareReg(s.group[0]);
}

bool isNumeric(const std::vector<Syllable> &syls) {
    for (const auto &s : syls) {
        if (s.kind == SylKind::Symbol) return false;
        if (s.kind == SylKind::Group && !isNumeric(s.group)) return false;
    }
    return true;
}

Operand buildOperand(RccAsmParser::OperandContext *ctx) {
    if (ctx->LBRACKET()) {
        fail("bracketed memory operands aren't part of this target's dialect - every load/store goes through "
             "the dedicated 'ldind*'/'stind*' mnemonics instead (see AlphaAssembler.h)");
    }
    std::vector<Syllable> syls = exprToSyllables(ctx->expr());
    Operand op;
    if (syls.size() == 1) {
        if (int r = bareReg(syls[0]); r >= 0) {
            op.kind = Operand::Reg;
            op.reg = r;
            return op;
        }
        if (int r = groupReg(syls[0]); r >= 0 && syls[0].sign == 0) {
            op.kind = Operand::RegDeferred;
            op.reg = r;
            return op;
        }
    }
    op.kind = isNumeric(syls) ? Operand::Imm : Operand::Mem;
    op.expr = std::move(syls);
    return op;
}

class ThrowingErrorListener : public antlr4::BaseErrorListener {
   public:
    void syntaxError(antlr4::Recognizer *, antlr4::Token *, size_t line, size_t col, const std::string &msg,
                      std::exception_ptr) override {
        fail(std::to_string(line) + ":" + std::to_string(col) + ": " + msg);
    }
};

Line buildLine(RccAsmParser::LineContext *ctx) {
    Line line;
    for (auto *lbl : ctx->label()) line.labels.push_back(lbl->IDENT()->getText());

    auto *stmt = ctx->statement();
    if (stmt == nullptr) {
        line.kind = Line::None;
        return line;
    }
    if (auto *assign = dynamic_cast<RccAsmParser::AssignStmtContext *>(stmt)) {
        line.kind = Line::Equate;
        line.equateName = assign->lhs->getText();
        line.exprList.push_back(exprToSyllables(assign->rhs));
        return line;
    }
    if (dynamic_cast<RccAsmParser::TimesStmtContext *>(stmt)) fail("'times' isn't part of this target's dialect");
    auto *instr = dynamic_cast<RccAsmParser::InstrStmtContext *>(stmt);
    if (!instr) fail("internal: unhandled statement alternative");

    std::string mnemonic = toLower(instr->mnemonic->getText());
    if (mnemonic == "org") {
        line.kind = Line::Org;
        if (instr->operand().empty() || instr->operand(0)->LBRACKET()) fail("'org' needs a plain address operand");
        line.exprList.push_back(exprToSyllables(instr->operand(0)->expr()));
        return line;
    }
    if (mnemonic == "dw" || mnemonic == "db" || mnemonic == "dh") {
        line.kind = mnemonic == "dw" ? Line::Dw : mnemonic == "dh" ? Line::Dh : Line::Db;
        for (auto *op : instr->operand()) {
            if (op->LBRACKET()) fail("'" + mnemonic + "' operand can't be bracketed");
            line.exprList.push_back(exprToSyllables(op->expr()));
        }
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

// A pending fixup, turned into an ObjRelocation once assembly is done.
struct Relocation {
    uint32_t offset;
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
                return;
            case Line::Org:
                orgBase_ = evalExpr(line.exprList.at(0));
                return;
            case Line::Dw:
                for (const auto &e : line.exprList) emitAddress32(e);
                return;
            case Line::Db:
                for (const auto &e : line.exprList) emit8(static_cast<uint8_t>(evalExpr(e)));
                return;
            case Line::Dh:
                for (const auto &e : line.exprList) emit16(static_cast<uint16_t>(evalExpr(e)));
                return;
            case Line::Instr:
                encodeInstr(line.mnemonic, line.operands);
                return;
        }
    }

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

    uint32_t resolveSyllable(const Syllable &syl, bool *wasExternal, bool *wasLocalAddress) {
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

    // --- word emission (Alpha is little-endian) -------------------------
    void emit8(uint8_t v) {
        bytes_.push_back(v);
        offset_ += 1;
    }
    void emit16(uint16_t v) {
        emit8(static_cast<uint8_t>(v & 0xFF));
        emit8(static_cast<uint8_t>((v >> 8) & 0xFF));
    }
    void emit32(uint32_t v) {
        emit16(static_cast<uint16_t>(v & 0xFFFF));
        emit16(static_cast<uint16_t>((v >> 16) & 0xFFFF));
    }

    void emitAddress32(const std::vector<Syllable> &expr) {
        std::string externalName;
        bool needsBaseShift = false;
        uint32_t v = evalExpr(expr, &externalName, &needsBaseShift);
        if (stage_ == 2 && allowExternal_) {
            if (needsBaseShift) relocations_.push_back({offset_, "..", kAlphaRelocAbs32});
            if (!externalName.empty()) relocations_.push_back({offset_, externalName, kAlphaRelocAbs32});
        }
        emit32(v);
    }

    static uint8_t requireReg(const Operand &op, const std::string &mnemonic) {
        if (op.kind != Operand::Reg) fail("'" + mnemonic + "' needs a register operand");
        return static_cast<uint8_t>(op.reg);
    }
    static uint8_t requireRegDeferred(const Operand &op, const std::string &mnemonic) {
        if (op.kind != Operand::RegDeferred) fail("'" + mnemonic + "' needs a '(reg)' operand");
        return static_cast<uint8_t>(op.reg);
    }
    static const Operand &requireOperand(const std::vector<Operand> &ops, size_t i, const std::string &mnemonic) {
        if (ops.size() <= i) fail("'" + mnemonic + "': missing operand");
        return ops[i];
    }

    int16_t evalDisp16(const Operand &op) {
        if (op.kind == Operand::Reg || op.kind == Operand::RegDeferred) fail("expected a displacement, not a register");
        int32_t v = static_cast<int32_t>(evalExpr(op.expr));
        if (v < -32768 || v > 32767) fail("displacement out of 16-bit signed range");
        return static_cast<int16_t>(v);
    }

    // --- instruction formats ---------------------------------------------
    // Memory/Jump format: opcode(6) Ra(5) Rb(5) disp-or-action-and-hint(16).
    void emitMem(uint8_t opcode, uint8_t ra, uint8_t rb, uint16_t disp16) {
        emit32((static_cast<uint32_t>(opcode) << 26) | (static_cast<uint32_t>(ra) << 21) |
               (static_cast<uint32_t>(rb) << 16) | disp16);
    }
    // Branch format: opcode(6) Ra(5) disp21 (word-granular, PC-relative to the next
    // instruction).
    void emitBranch(uint8_t opcode, uint8_t ra, int32_t wordDisp) {
        emit32((static_cast<uint32_t>(opcode) << 26) | (static_cast<uint32_t>(ra) << 21) |
               (static_cast<uint32_t>(wordDisp) & 0x1FFFFFu));
    }
    // Operate format, register form: opcode(6) Ra(5) Rb(5) SBZ(3)=0 lit=0(1) function(7) Rc(5).
    void emitOperateReg(uint8_t opcode, uint8_t ra, uint8_t rb, uint8_t function, uint8_t rc) {
        emit32((static_cast<uint32_t>(opcode) << 26) | (static_cast<uint32_t>(ra) << 21) |
               (static_cast<uint32_t>(rb) << 16) | (static_cast<uint32_t>(function) << 5) | rc);
    }
    // Operate format, literal form: opcode(6) Ra(5) lit8(8) lit=1(1) function(7) Rc(5).
    void emitOperateLit(uint8_t opcode, uint8_t ra, uint8_t lit8, uint8_t function, uint8_t rc) {
        emit32((static_cast<uint32_t>(opcode) << 26) | (static_cast<uint32_t>(ra) << 21) |
               (static_cast<uint32_t>(lit8) << 13) | (1u << 12) | (static_cast<uint32_t>(function) << 5) | rc);
    }

    void encodeLdaFamily(uint8_t opcode, uint16_t relocKind, const std::vector<Operand> &ops,
                          const std::string &mnemonic) {
        uint8_t rd = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        uint8_t rb = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
        const Operand &dispOp = requireOperand(ops, 2, mnemonic);
        if (dispOp.kind == Operand::Mem) {
            std::string externalName;
            bool needsBaseShift = false;
            uint32_t full = evalExpr(dispOp.expr, &externalName, &needsBaseShift);
            // %hi/%lo: the low half's own sign bit (disp15) decides whether the high half
            // needs its "+1 high adjust" - see AlphaAssembler.h's own comment.
            uint16_t disp16 = opcode == 0x09 /* ldah */
                                   ? static_cast<uint16_t>(((full >> 16) + ((full & 0x8000u) ? 1 : 0)) & 0xFFFFu)
                                   : static_cast<uint16_t>(full & 0xFFFFu);
            if (stage_ == 2 && allowExternal_) {
                if (needsBaseShift) relocations_.push_back({offset_, "..", relocKind});
                if (!externalName.empty()) relocations_.push_back({offset_, externalName, relocKind});
            }
            emitMem(opcode, rd, rb, disp16);
            return;
        }
        emitMem(opcode, rd, rb, static_cast<uint16_t>(static_cast<uint32_t>(evalDisp16(dispOp))));
    }

    void encodeMemAccess(uint8_t opcode, const std::vector<Operand> &ops, const std::string &mnemonic) {
        uint8_t base = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        uint8_t reg = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
        int16_t disp = 0;
        if (ops.size() > 2) disp = evalDisp16(requireOperand(ops, 2, mnemonic));
        emitMem(opcode, reg, base, static_cast<uint16_t>(static_cast<uint32_t>(disp)));
    }

    void encodeJumpFormat(uint8_t action, const std::vector<Operand> &ops, const std::string &mnemonic) {
        uint8_t rd = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        uint8_t rb = requireRegDeferred(requireOperand(ops, 1, mnemonic), mnemonic);
        emitMem(0x1A, rd, rb, static_cast<uint16_t>(static_cast<uint32_t>(action) << 14));
    }

    void encodeBranchInstr(uint8_t opcode, const Operand &regOp, const Operand &target, const std::string &mnemonic) {
        uint8_t ra = regOp.kind == Operand::Reg ? requireReg(regOp, mnemonic) : 31 /* zero */;
        if (target.kind != Operand::Mem) fail("'" + mnemonic + "': branch target must be a label");
        std::string externalName;
        uint32_t targetAddr = evalExpr(target.expr, &externalName);
        if (stage_ == 2 && !externalName.empty()) {
            fail("'" + mnemonic + "': branch target '" + externalName + "' isn't defined in this file");
        }
        int32_t rel = (static_cast<int64_t>(targetAddr) - static_cast<int64_t>(orgBase_ + offset_ + 4)) / 4;
        if (stage_ == 2 && (rel < -(1 << 20) || rel > (1 << 20) - 1)) {
            fail("'" + mnemonic + "': branch target out of 21-bit range");
        }
        emitBranch(opcode, ra, rel);
    }

    void encodeOperate(uint8_t opcode, uint8_t function, const std::vector<Operand> &ops,
                        const std::string &mnemonic) {
        uint8_t rd = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        uint8_t ra = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
        const Operand &third = requireOperand(ops, 2, mnemonic);
        if (third.kind == Operand::Reg) {
            emitOperateReg(opcode, ra, static_cast<uint8_t>(third.reg), function, rd);
            return;
        }
        if (third.kind != Operand::Imm) fail("'" + mnemonic + "': the third operand must be a register or a 0..255 literal");
        int32_t v = static_cast<int32_t>(evalExpr(third.expr));
        if (v < 0 || v > 255) fail("'" + mnemonic + "': literal operand out of 0..255 range");
        emitOperateLit(opcode, ra, static_cast<uint8_t>(v), function, rd);
    }

    void encodeInstr(const std::string &mnemonic, const std::vector<Operand> &ops) {
        if (mnemonic == "lda") {
            encodeLdaFamily(0x08, kAlphaRelocLow, ops, mnemonic);
            return;
        }
        if (mnemonic == "ldah") {
            encodeLdaFamily(0x09, kAlphaRelocHigh, ops, mnemonic);
            return;
        }
        static const std::unordered_map<std::string, uint8_t> memOps = {
            {"ldindbu", 0x0A}, {"ldindhu", 0x0C}, {"ldindl", 0x28}, {"ldindq", 0x29},
            {"stindb", 0x0E},  {"stindh", 0x0D},  {"stindl", 0x2C}, {"stindq", 0x2D},
        };
        if (auto it = memOps.find(mnemonic); it != memOps.end()) {
            encodeMemAccess(it->second, ops, mnemonic);
            return;
        }
        if (mnemonic == "jmp" || mnemonic == "jsr" || mnemonic == "ret") {
            encodeJumpFormat(mnemonic == "jmp" ? 0 : mnemonic == "jsr" ? 1 : 2, ops, mnemonic);
            return;
        }
        if (mnemonic == "br") {
            if (ops.size() != 1) fail("'br' takes one label operand");
            encodeBranchInstr(0x30, Operand{}, ops[0], mnemonic);
            return;
        }
        static const std::unordered_map<std::string, uint8_t> condBranch = {
            {"beq", 0x39}, {"bne", 0x3D}, {"blt", 0x3A}, {"ble", 0x3B}, {"bgt", 0x3F}, {"bge", 0x3E},
        };
        if (auto it = condBranch.find(mnemonic); it != condBranch.end()) {
            if (ops.size() != 2) fail("'" + mnemonic + "' takes a register and a label operand");
            encodeBranchInstr(it->second, ops[0], ops[1], mnemonic);
            return;
        }
        static const std::unordered_map<std::string, std::pair<uint8_t, uint8_t>> operate = {
            {"addq", {0x10, 0x20}},   {"subq", {0x10, 0x29}},  {"cmpeq", {0x10, 0x2D}}, {"cmplt", {0x10, 0x4D}},
            {"cmple", {0x10, 0x6D}},  {"cmpult", {0x10, 0x1D}}, {"cmpule", {0x10, 0x3D}},
            {"and", {0x11, 0x00}},   {"bis", {0x11, 0x20}},   {"xor", {0x11, 0x40}}, {"ornot", {0x11, 0x28}},
            {"sll", {0x12, 0x39}},   {"srl", {0x12, 0x34}},   {"sra", {0x12, 0x3C}},
            {"mulq", {0x13, 0x20}},  {"umulh", {0x13, 0x30}},
        };
        if (auto it = operate.find(mnemonic); it != operate.end()) {
            encodeOperate(it->second.first, it->second.second, ops, mnemonic);
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
    obj.machineType = kAlphaCoffMachine;
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
        obj.relocations.push_back({r.offset, r.symbolName, r.kind});
    }
    return obj;
}

}  // namespace

std::string assembleAlpha(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/false);
    assembler.run(lines);
    const auto &bytes = assembler.bytes();
    return std::string(bytes.begin(), bytes.end());
}

ObjectFile assembleAlphaObject(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/true);
    assembler.run(lines);
    return buildObjectFile(assembler);
}

}  // namespace rcc
