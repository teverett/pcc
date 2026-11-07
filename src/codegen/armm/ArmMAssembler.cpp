#include "ArmMAssembler.h"

#include <algorithm>
#include <cstdint>
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

[[noreturn]] void fail(const std::string &msg) { throw std::runtime_error("ArmMAssembler: " + msg); }

// --- Parsing, via the one shared grammar/PccAsm.g4 -----------------------
enum class SylKind { Number, Symbol, Group };
struct Syllable {
    char sign = 0;
    SylKind kind;
    std::string text;
    std::vector<Syllable> group;
};

// Reg: a bare register name. Imm: a bare signed number. Mem: a label or
// label+N, resolved to a full 32-bit address (split into halves for movw/movt).
struct Operand {
    enum Kind { Reg, Imm, Mem } kind = Imm;
    int reg = -1;
    std::vector<Syllable> expr;  // Imm/Mem
};

struct Line {
    std::vector<std::string> labels;
    enum Kind { None, Equate, Org, Dw, Instr } kind = None;
    std::string equateName;
    std::vector<std::vector<Syllable>> exprList;  // Equate (1)/Org (1)/Dw (N)
    // 1 for db, 2 for dh, 4 for dw; long long is emitted as two dw elements.
    int dataWidth = 4;
    std::string mnemonic;                           // Instr
    std::vector<Operand> operands;                  // Instr
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

// Maps the subset of registers this dialect names to their real ARM numbers
// (sp=13, lr=14), so encoded bits match genuine Thumb object code. Returns -1
// if not found.
int regNumber(const std::string &lower) {
    static const std::unordered_map<std::string, int> names = {
        {"r0", 0}, {"r1", 1}, {"r2", 2}, {"r3", 3}, {"r4", 4}, {"r5", 5},
        {"r6", 6}, {"r7", 7},  {"sp", 13}, {"lr", 14},
    };
    auto it = names.find(lower);
    return it == names.end() ? -1 : it->second;
}

Operand buildOperand(PccAsmParser::OperandContext *ctx) {
    if (ctx->LBRACKET()) {
        fail(
            "bracketed memory operands aren't part of this target's dialect - ARM memory access always goes "
            "through the dedicated 'ldind'/'stind' mnemonics instead, and register lists through 'push'/'pop'/"
            "'pushlr'/'poppc' (see ArmMCodegen.h)");
    }
    std::vector<Syllable> syls = exprToSyllables(ctx->expr());
    Operand op;
    if (syls.size() == 1 && syls[0].sign == 0 && syls[0].kind == SylKind::Symbol) {
        if (int r = regNumber(toLower(syls[0].text)); r >= 0) {
            op.kind = Operand::Reg;
            op.reg = r;
            return op;
        }
    }
    bool allNumbers = true;
    for (const auto &s : syls) {
        if (s.kind != SylKind::Number) {
            allNumbers = false;
            break;
        }
    }
    op.kind = allNumbers ? Operand::Imm : Operand::Mem;
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
    if (dynamic_cast<PccAsmParser::TimesStmtContext *>(stmt)) fail("'times' isn't part of this target's dialect");
    auto *instr = dynamic_cast<PccAsmParser::InstrStmtContext *>(stmt);
    if (!instr) fail("internal: unhandled statement alternative");

    std::string mnemonic = toLower(instr->mnemonic->getText());
    if (mnemonic == "org") {
        line.kind = Line::Org;
        if (instr->operand().empty() || instr->operand(0)->LBRACKET()) fail("'org' needs a plain address operand");
        line.exprList.push_back(exprToSyllables(instr->operand(0)->expr()));
        return line;
    }
    if (mnemonic == "dw" || mnemonic == "dh" || mnemonic == "db") {
        line.kind = Line::Dw;
        line.dataWidth = mnemonic == "db" ? 1 : mnemonic == "dh" ? 2 : 4;
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

// A pending fixup turned into a real ObjRelocation once assembly is done.
// symbolName is either an unresolved reference or ".." for a same-file
// address's base-shift.
struct Relocation {
    uint32_t offset;
    std::string symbolName;
    uint16_t kind;
};

// --- Two-pass assembly ----------------------------------------------------
// A dialect mnemonic may expand to more than one real instruction (the
// conditional branches below emit 6 bytes); each encode function advances
// offset_ by its own real size, in both passes.
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
                for (const auto &e : line.exprList) {
                    uint32_t v = evalExpr(e);
                    if (line.dataWidth == 1) {
                        emit8(static_cast<uint8_t>(v));
                    } else if (line.dataWidth == 2) {
                        emit16(static_cast<uint16_t>(v));
                    } else {
                        emit32(v);
                    }
                }
                return;
            case Line::Instr:
                encodeInstr(line.mnemonic, line.operands);
                return;
        }
    }

    // Once allowExternal_ is set, any local address reference also needs a
    // relocation (for the base-shift), not just a genuinely external one.
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

    // --- byte/halfword/word emission ------------------------------------
    void emit8(uint8_t b) {
        bytes_.push_back(b);
        offset_ += 1;
    }
    void emit16(uint16_t w) {
        bytes_.push_back(static_cast<uint8_t>(w & 0xFF));
        bytes_.push_back(static_cast<uint8_t>((w >> 8) & 0xFF));
        offset_ += 2;
    }
    void emit32(uint32_t v) {
        bytes_.push_back(static_cast<uint8_t>(v & 0xFF));
        bytes_.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        bytes_.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
        bytes_.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
        offset_ += 4;
    }

    static uint8_t requireReg(const Operand &op, const std::string &mnemonic) {
        if (op.kind != Operand::Reg) fail("'" + mnemonic + "' needs a register operand");
        return static_cast<uint8_t>(op.reg);
    }
    static const Operand &requireOperand(const std::vector<Operand> &ops, size_t i, const std::string &mnemonic) {
        if (ops.size() <= i) fail("'" + mnemonic + "': missing operand");
        return ops[i];
    }

    uint32_t evalImm(const Operand &op) {
        if (op.kind == Operand::Reg) fail("expected an immediate or label operand, not a register");
        return evalExpr(op.expr);
    }

    // A plain number is used as-is (already split by ArmMCodegen.cpp); a
    // label operand's full address is split into its lower (movw) or upper
    // (movt) 16 bits here.
    uint16_t evalHiLo(const Operand &op, bool hi, std::string *externalName = nullptr,
                       bool *needsBaseShift = nullptr) {
        if (op.kind == Operand::Reg) fail("expected an immediate or label operand, not a register");
        uint32_t full = evalExpr(op.expr, externalName, needsBaseShift);
        if (op.kind == Operand::Mem) return static_cast<uint16_t>(hi ? (full >> 16) & 0xFFFFu : full & 0xFFFFu);
        return static_cast<uint16_t>(full & 0xFFFFu);
    }

    // --- Thumb-1/Thumb-2 instruction encodings --------------------------

    // MOVS Rd,#imm8 (T1): 0010 0 Rd imm8.
    void encodeMovs(const std::vector<Operand> &ops) {
        uint8_t rd = requireReg(requireOperand(ops, 0, "movs"), "movs");
        uint32_t imm = evalImm(requireOperand(ops, 1, "movs"));
        if (imm > 0xFF) fail("'movs': immediate must fit in 8 bits");
        emit16(static_cast<uint16_t>(0x2000 | (rd << 8) | imm));
    }

    // MOVW/MOVT Rd,#imm16 (T3/T1 32-bit encodings) - imm16 = imm4:i:imm3:imm8,
    // a non-contiguous split the linker's patch functions must also honor.
    void encodeMovWT(bool isMovt, const std::vector<Operand> &ops) {
        const std::string &mnemonic = isMovt ? "movt" : "movw";
        uint8_t rd = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        const Operand &immOp = requireOperand(ops, 1, mnemonic);
        std::string externalName;
        bool needsBaseShift = false;
        uint16_t imm16 = evalHiLo(immOp, /*hi=*/isMovt, &externalName, &needsBaseShift);
        if (stage_ == 2 && allowExternal_ && immOp.kind == Operand::Mem) {
            uint16_t kind = isMovt ? kArmMRelocMovTHi : kArmMRelocMovWLo;
            if (needsBaseShift) relocations_.push_back({offset_, "..", kind});
            if (!externalName.empty()) relocations_.push_back({offset_, externalName, kind});
        }
        uint16_t imm8 = imm16 & 0xFF;
        uint16_t imm3 = (imm16 >> 8) & 0x7;
        uint16_t i = (imm16 >> 11) & 0x1;
        uint16_t imm4 = (imm16 >> 12) & 0xF;
        uint16_t base = isMovt ? 0xF2C0 : 0xF240;
        emit16(static_cast<uint16_t>(base | (i << 10) | imm4));
        emit16(static_cast<uint16_t>((imm3 << 12) | (rd << 8) | imm8));
    }

    // MOV Rd,Rm (T1 "MOV register", low-register case: D=0): 0100
    // 0110 D Rm(4) Rd(3).
    void encodeMov(const std::vector<Operand> &ops) {
        uint8_t rd = requireReg(requireOperand(ops, 0, "mov"), "mov");
        uint8_t rm = requireReg(requireOperand(ops, 1, "mov"), "mov");
        emit16(static_cast<uint16_t>(0x4600 | (rm << 3) | rd));
    }

    // ADDS Rd,Rn,Rm (T1 register form): 0001100 Rm Rn Rd.
    // ADDS Rd,Rn,#imm3 (T1 immediate form, imm3 0-7): 0001110 imm3 Rn Rd.
    // SUBS Rd,Rn,Rm (T1 register form): 0001101 Rm Rn Rd.
    void encodeAddSubReg3(bool isSub, const std::vector<Operand> &ops) {
        const std::string &mnemonic = isSub ? "subs" : "adds";
        uint8_t rd = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        uint8_t rn = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
        const Operand &third = requireOperand(ops, 2, mnemonic);
        if (third.kind == Operand::Reg) {
            uint16_t base = isSub ? 0x1A00 : 0x1800;
            emit16(static_cast<uint16_t>(base | (static_cast<uint8_t>(third.reg) << 6) | (rn << 3) | rd));
            return;
        }
        if (isSub) fail("'subs': immediate form isn't part of this target's dialect");
        uint32_t imm3 = evalImm(third);
        if (imm3 > 7) fail("'adds': immediate form only supports 0-7");
        emit16(static_cast<uint16_t>(0x1C00 | (imm3 << 6) | (rn << 3) | rd));
    }

    // CMP Rn,Rm (T1 register form, ALU-op table): 010000 1010 Rm Rn.
    // CMP Rn,#imm8 (T1 immediate form): 00101 Rn imm8.
    void encodeCmp(const std::vector<Operand> &ops) {
        uint8_t rn = requireReg(requireOperand(ops, 0, "cmp"), "cmp");
        const Operand &second = requireOperand(ops, 1, "cmp");
        if (second.kind == Operand::Reg) {
            emit16(static_cast<uint16_t>(0x4280 | (static_cast<uint8_t>(second.reg) << 3) | rn));
            return;
        }
        uint32_t imm = evalImm(second);
        if (imm > 0xFF) fail("'cmp': immediate must fit in 8 bits");
        emit16(static_cast<uint16_t>(0x2800 | (rn << 8) | imm));
    }

    // The shared "ALU operations" 2-operand Thumb-1 table (T1):
    // 010000 op(4) Rm Rdn - AND=0000, EOR=0001, LSL=0010, LSR=0011,
    // NEG(RSB#0)=1001, ORR=1100, MUL=1101, MVN=1111. `dest` is the
    // `Rdn`/`Rd` field, `src` the `Rm` field.
    void encodeAluOp2(uint8_t op, uint8_t dest, uint8_t src) {
        emit16(static_cast<uint16_t>(0x4000 | (op << 6) | (src << 3) | dest));
    }
    void encodeAluOp2(uint8_t op, const std::vector<Operand> &ops, const std::string &mnemonic) {
        uint8_t dest = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        uint8_t src = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
        encodeAluOp2(op, dest, src);
    }

    // LSL Rd,Rm,#imm5 (T1 "Shift (immediate)" form, imm5 != 0):
    // 00000 imm5 Rm Rd.
    void encodeLsli(const std::vector<Operand> &ops) {
        uint8_t rd = requireReg(requireOperand(ops, 0, "lsli"), "lsli");
        uint8_t rm = requireReg(requireOperand(ops, 1, "lsli"), "lsli");
        uint32_t imm5 = evalImm(requireOperand(ops, 2, "lsli"));
        if (imm5 < 1 || imm5 > 31) fail("'lsli': immediate must be 1-31");
        emit16(static_cast<uint16_t>((imm5 << 6) | (rm << 3) | rd));
    }

    // LDR/STR Rt,[Rn,#0] (T1 immediate form, imm5=0) - this dialect's
    // own `ldind`/`stind`, always a fixed zero displacement.
    void encodeLdStInd(bool isLoad, const std::vector<Operand> &ops) {
        const std::string &mnemonic = isLoad ? "ldind" : "stind";
        uint8_t rn = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        uint8_t rt = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
        uint16_t base = isLoad ? 0x6800 : 0x6000;
        emit16(static_cast<uint16_t>(base | (rn << 3) | rt));
    }

    // LDRB/STRB Rt,[Rn,#0] (T1, imm5=0): 0111 1/0 00000 Rn Rt.
    // LDRH/STRH Rt,[Rn,#0] (T1, imm5=0): 1000 1/0 00000 Rn Rt.
    // Always zero-extending; a signed value is re-sign-extended afterward via
    // ArmMCodegen.cpp's lsl/asr idiom since no sign-extending form exists here.
    void encodeLdStIndNarrow(bool isLoad, bool isByte, const std::vector<Operand> &ops) {
        const std::string &mnemonic = isLoad ? (isByte ? "ldindb" : "ldindh") : (isByte ? "stindb" : "stindh");
        uint8_t rn = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        uint8_t rt = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
        uint16_t base = isByte ? (isLoad ? 0x7800 : 0x7000) : (isLoad ? 0x8800 : 0x8000);
        emit16(static_cast<uint16_t>(base | (rn << 3) | rt));
    }

    // PUSH {Rd} / POP {Rd} (T1, single-register list, M/P=0):
    // 1011010 0 reglist / 1011110 0 reglist.
    void encodePushPop(bool isPush, const std::vector<Operand> &ops) {
        const std::string &mnemonic = isPush ? "push" : "pop";
        uint8_t r = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        if (r > 7) fail("'" + mnemonic + "' only supports r0-r7 in this dialect");
        uint16_t base = isPush ? 0xB400 : 0xBC00;
        emit16(static_cast<uint16_t>(base | (1u << r)));
    }

    // BL/wide-B (T1/T4): both use the S/I1/I2/J1/J2 offset-mangling scheme
    // (imm32 = S:I1:I2:imm10:imm11:0, I1 = NOT(J1 XOR S), I2 = NOT(J2 XOR S));
    // only the fixed bits distinguishing BL from B.W differ.
    // A same-object target needs no relocation (both it and this instruction
    // shift by the same link base); only a genuinely external target does.
    void encodeWideBranch(uint16_t halfword2Base, const Operand &target, const std::string &mnemonic) {
        if (target.kind != Operand::Mem) fail("'" + mnemonic + "': target must be a label");
        std::string externalName;
        uint32_t targetAddr = evalExpr(target.expr, &externalName);
        uint32_t pcRelBase = orgBase_ + offset_ + 4;
        if (stage_ == 2 && allowExternal_ && !externalName.empty()) {
            relocations_.push_back({offset_, externalName, kArmMRelocBranch});
            emit16(0xF000);
            emit16(halfword2Base);
            return;
        }
        int32_t off = static_cast<int32_t>(targetAddr) - static_cast<int32_t>(pcRelBase);
        if (stage_ == 2 && (off & 1)) fail("internal: odd branch offset");
        int32_t u = off >> 1;  // off is always even
        uint32_t s = (static_cast<uint32_t>(u) >> 23) & 1;
        uint32_t i1 = (static_cast<uint32_t>(u) >> 22) & 1;
        uint32_t i2 = (static_cast<uint32_t>(u) >> 21) & 1;
        uint32_t imm10 = (static_cast<uint32_t>(u) >> 11) & 0x3FF;
        uint32_t imm11 = static_cast<uint32_t>(u) & 0x7FF;
        uint32_t j1 = (i1 ^ s) ^ 1;
        uint32_t j2 = (i2 ^ s) ^ 1;
        emit16(static_cast<uint16_t>(0xF000 | (s << 10) | imm10));
        emit16(static_cast<uint16_t>(halfword2Base | (j1 << 13) | (j2 << 11) | imm11));
    }
    void encodeBl(const std::vector<Operand> &ops) { encodeWideBranch(0xD000, requireOperand(ops, 0, "bl"), "bl"); }
    void encodeBWide(const std::vector<Operand> &ops) { encodeWideBranch(0x9000, requireOperand(ops, 0, "b"), "b"); }

    // BLX Rm (T1): 0100 0111 1 Rm(4) 000 - branches to Rm and sets lr, like
    // bl does for a label target.
    void encodeBlx(const std::vector<Operand> &ops) {
        uint8_t rm = requireReg(requireOperand(ops, 0, "blx"), "blx");
        emit16(static_cast<uint16_t>(0x4780 | (rm << 3)));
    }

    // Each conditional-branch mnemonic expands to a short Bcc on the inverted
    // condition (fixed +2-byte displacement, landing just past the B.W that
    // follows, so it fires - skipping the B.W - only when the real condition
    // is false) and then the real wide unconditional B.W <target>.
    // Short Bcc (T1): 1101 cond imm8.
    void encodeConditionalBranch(uint8_t invertedCond, const std::vector<Operand> &ops, const std::string &mnemonic) {
        const Operand &target = requireOperand(ops, 0, mnemonic);
        emit16(static_cast<uint16_t>(0xD000 | (invertedCond << 8) | 1));
        encodeWideBranch(0x9000, target, mnemonic);
    }

    // Exactly the mnemonics ArmMCodegen.cpp emits, not a general Thumb encoder.
    void encodeInstr(const std::string &mnemonic, const std::vector<Operand> &ops) {
        if (mnemonic == "movs") return encodeMovs(ops);
        if (mnemonic == "movw") return encodeMovWT(/*isMovt=*/false, ops);
        if (mnemonic == "movt") return encodeMovWT(/*isMovt=*/true, ops);
        if (mnemonic == "mov") return encodeMov(ops);
        if (mnemonic == "adds") return encodeAddSubReg3(/*isSub=*/false, ops);
        if (mnemonic == "subs") return encodeAddSubReg3(/*isSub=*/true, ops);
        if (mnemonic == "cmp") return encodeCmp(ops);
        if (mnemonic == "and") return encodeAluOp2(0b0000, ops, mnemonic);
        if (mnemonic == "eor") return encodeAluOp2(0b0001, ops, mnemonic);
        if (mnemonic == "lsl") return encodeAluOp2(0b0010, ops, mnemonic);
        if (mnemonic == "lsr") return encodeAluOp2(0b0011, ops, mnemonic);
        if (mnemonic == "asr") return encodeAluOp2(0b0100, ops, mnemonic);
        if (mnemonic == "adc") return encodeAluOp2(0b0101, ops, mnemonic);
        if (mnemonic == "sbc") return encodeAluOp2(0b0110, ops, mnemonic);
        if (mnemonic == "neg") return encodeAluOp2(0b1001, ops, mnemonic);
        if (mnemonic == "orr") return encodeAluOp2(0b1100, ops, mnemonic);
        if (mnemonic == "muls") return encodeAluOp2(0b1101, ops, mnemonic);
        if (mnemonic == "mvn") return encodeAluOp2(0b1111, ops, mnemonic);
        if (mnemonic == "lsli") return encodeLsli(ops);
        if (mnemonic == "ldind") return encodeLdStInd(/*isLoad=*/true, ops);
        if (mnemonic == "stind") return encodeLdStInd(/*isLoad=*/false, ops);
        if (mnemonic == "ldindb") return encodeLdStIndNarrow(/*isLoad=*/true, /*isByte=*/true, ops);
        if (mnemonic == "stindb") return encodeLdStIndNarrow(/*isLoad=*/false, /*isByte=*/true, ops);
        if (mnemonic == "ldindh") return encodeLdStIndNarrow(/*isLoad=*/true, /*isByte=*/false, ops);
        if (mnemonic == "stindh") return encodeLdStIndNarrow(/*isLoad=*/false, /*isByte=*/false, ops);
        if (mnemonic == "push") return encodePushPop(/*isPush=*/true, ops);
        if (mnemonic == "pop") return encodePushPop(/*isPush=*/false, ops);
        if (mnemonic == "pushlr") {
            if (!ops.empty()) fail("'pushlr' takes no operand");
            emit16(0xB500);
            return;
        }
        if (mnemonic == "poppc") {
            if (!ops.empty()) fail("'poppc' takes no operand");
            emit16(0xBD00);
            return;
        }
        if (mnemonic == "bl") return encodeBl(ops);
        if (mnemonic == "blx") return encodeBlx(ops);
        if (mnemonic == "b") return encodeBWide(ops);
        // Inverted Thumb condition codes, since encodeConditionalBranch's short
        // Bcc tests the opposite of the mnemonic's own condition.
        static const std::unordered_map<std::string, uint8_t> condBranch = {
            {"beq", 0x1}, {"bne", 0x0}, {"blt", 0xA}, {"bge", 0xB}, {"bgt", 0xD}, {"ble", 0xC},
            {"blo", 0x2}, {"bhs", 0x3}, {"bhi", 0x9}, {"bls", 0x8},
        };
        if (auto it = condBranch.find(mnemonic); it != condBranch.end()) {
            encodeConditionalBranch(it->second, ops, mnemonic);
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

// Builds the format-neutral ObjectFile from a completed object-mode Assembler.
ObjectFile buildObjectFile(const Assembler &a) {
    ObjectFile obj;
    obj.machineType = kArmMCoffMachine;
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

std::string assembleArmM(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/false);
    assembler.run(lines);
    const auto &bytes = assembler.bytes();
    return std::string(bytes.begin(), bytes.end());
}

ObjectFile assembleArmMObject(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/true);
    assembler.run(lines);
    return buildObjectFile(assembler);
}

}  // namespace pcc
