#include "ArmAssembler.h"

#include <algorithm>
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

[[noreturn]] void fail(const std::string &msg) { throw std::runtime_error("ArmAssembler: " + msg); }

// --- Parsing, via the shared grammar/RccAsm.g4 ----------------------------
enum class SylKind { Number, Symbol, Group };
struct Syllable {
    char sign = 0;
    SylKind kind;
    std::string text;
    std::vector<Syllable> group;
};

// A bare register name is `Reg`; a bare signed number is `Imm`; anything
// else (a label or `label+N`) is `Mem`, an address resolved to its full
// 32-bit value (split into lower/upper half for `movw`/`movt`).
struct Operand {
    enum Kind { Reg, Imm, Mem } kind = Imm;
    int reg = -1;
    std::vector<Syllable> expr;  // Imm/Mem
};

struct Line {
    std::vector<std::string> labels;
    enum Kind { None, Equate, Org, Dw, Db, Dh, Instr } kind = None;
    std::string equateName;
    std::vector<std::vector<Syllable>> exprList;  // Equate (1)/Org (1)/Dw (N)
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

// Register numbers match real ARM (`fp` aliases r11), so encoded bit
// patterns match genuine A32 object code. Returns -1 if `name` isn't one
// of these. r4-r10/r12 are scratch registers for the `long long`
// multi-word arithmetic in ArmCodegen.cpp's genWide*() routines, which
// never cross a call boundary, so no caller/callee-saved convention
// applies to them.
int regNumber(const std::string &lower) {
    static const std::unordered_map<std::string, int> names = {
        {"r0", 0}, {"r1", 1}, {"r2", 2}, {"r3", 3}, {"r4", 4}, {"r5", 5}, {"r6", 6}, {"r7", 7}, {"r8", 8},
        {"r9", 9}, {"r10", 10}, {"fp", 11}, {"r12", 12}, {"sp", 13}, {"lr", 14},
    };
    auto it = names.find(lower);
    return it == names.end() ? -1 : it->second;
}

Operand buildOperand(RccAsmParser::OperandContext *ctx) {
    if (ctx->LBRACKET()) {
        fail(
            "bracketed memory operands aren't part of this target's dialect - ARM memory access always goes "
            "through the dedicated 'ldind'/'stind' mnemonics instead (see ArmCodegen.h)");
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
        line.kind = mnemonic == "dw" ? Line::Dw : mnemonic == "db" ? Line::Db : Line::Dh;
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

// A pending fixup turned into an ObjRelocation once assembly is done.
// `symbolName` is the unresolved reference, or the reserved pseudo-symbol
// ".." for a same-file address's base-shift; `kind` is one of
// kArmRelocRel24/MovWLo/MovTHi.
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
                for (const auto &e : line.exprList) emit32(evalExpr(e));
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

    // Once `allowExternal_` is set, any local address reference, not just
    // an external one, needs a relocation.
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

    // --- word emission (little-endian) ----------------------------------
    void emit32(uint32_t v) {
        bytes_.push_back(static_cast<uint8_t>(v & 0xFF));
        bytes_.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        bytes_.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
        bytes_.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
        offset_ += 4;
    }
    // `db`/`dh` - byte/halfword data for narrower-than-`int` global storage.
    void emit8(uint8_t v) {
        bytes_.push_back(v);
        offset_ += 1;
    }
    void emit16(uint16_t v) {
        bytes_.push_back(static_cast<uint8_t>(v & 0xFF));
        bytes_.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        offset_ += 2;
    }

    // Callers already range-check the result (8-bit data-processing
    // immediate, 5-bit shift amount), so no range check is repeated here.
    uint32_t evalImm(const Operand &op) {
        if (op.kind == Operand::Reg) fail("expected an immediate or label operand, not a register");
        return evalExpr(op.expr);
    }

    // `movw`/`movt` immediate operand: a plain number is used as-is; a
    // label (`Mem`) has its resolved address split into its lower
    // (`movw`) or upper (`movt`) 16 bits automatically.
    uint16_t evalHiLo(const Operand &op, bool hi, std::string *externalName = nullptr,
                      bool *needsBaseShift = nullptr) {
        if (op.kind == Operand::Reg) fail("expected an immediate or label operand, not a register");
        uint32_t full = evalExpr(op.expr, externalName, needsBaseShift);
        if (op.kind == Operand::Mem) return static_cast<uint16_t>(hi ? (full >> 16) & 0xFFFFu : full & 0xFFFFu);
        return static_cast<uint16_t>(full & 0xFFFFu);
    }

    static uint8_t requireReg(const Operand &op, const std::string &mnemonic) {
        if (op.kind != Operand::Reg) fail("'" + mnemonic + "' needs a register operand");
        return static_cast<uint8_t>(op.reg);
    }
    static const Operand &requireOperand(const std::vector<Operand> &ops, size_t i, const std::string &mnemonic) {
        if (ops.size() <= i) fail("'" + mnemonic + "': missing operand");
        return ops[i];
    }

    // --- condition-code suffixes -----------------------------------------
    // Every A32 instruction's top 4 bits are its condition field, spelled
    // here as a mnemonic suffix (`moveq`, `beq`, ...).
    static uint8_t condOf(const std::string &suffix) {
        static const std::unordered_map<std::string, uint8_t> conds = {
            {"", 0xE}, {"eq", 0x0}, {"ne", 0x1}, {"lt", 0xB}, {"ge", 0xA}, {"le", 0xD}, {"gt", 0xC},
            // Unsigned counterparts: real ARM condition codes HS/LO/HI/LS.
            {"hs", 0x2}, {"lo", 0x3}, {"hi", 0x8}, {"ls", 0x9},
        };
        auto it = conds.find(suffix);
        if (it == conds.end()) fail("unsupported condition suffix '" + suffix + "'");
        return it->second;
    }

    // --- A32 instruction formats -----------------------------------------
    // Data-processing, unshifted register operand2 (Rm):
    // cond(4) 00 0 opcode(4) S(1) Rn(4) Rd(4) 00000000 Rm(4).
    void emitDPReg(uint8_t cond, uint8_t opcode, uint8_t s, uint8_t rn, uint8_t rd, uint8_t rm) {
        emit32((static_cast<uint32_t>(cond) << 28) | (static_cast<uint32_t>(opcode) << 21) |
               (static_cast<uint32_t>(s) << 20) | (static_cast<uint32_t>(rn) << 16) |
               (static_cast<uint32_t>(rd) << 12) | rm);
    }
    // Data-processing, unrotated 8-bit immediate operand2:
    // cond(4) 00 1 opcode(4) S(1) Rn(4) Rd(4) 0000 imm8(8).
    void emitDPImm(uint8_t cond, uint8_t opcode, uint8_t s, uint8_t rn, uint8_t rd, uint32_t imm8) {
        if (imm8 > 0xFF) fail("data-processing immediate out of 8-bit range");
        emit32((static_cast<uint32_t>(cond) << 28) | (1u << 25) | (static_cast<uint32_t>(opcode) << 21) |
               (static_cast<uint32_t>(s) << 20) | (static_cast<uint32_t>(rn) << 16) |
               (static_cast<uint32_t>(rd) << 12) | imm8);
    }
    // `add`/`sub rd,rn,op2`, op2 a register or an immediate.
    void encodeAddSub(uint8_t cond, uint8_t opcode, const std::vector<Operand> &ops, const std::string &mnemonic) {
        encodeAddSubS(cond, opcode, 0, ops, mnemonic);
    }
    // `s` sets the S bit (update flags, specifically carry/borrow) -
    // needed on the low-word half of a `long long` add/sub chain so the
    // high-word `adc`/`sbc` reads the carry/borrow out of it.
    void encodeAddSubS(uint8_t cond, uint8_t opcode, uint8_t s, const std::vector<Operand> &ops,
                        const std::string &mnemonic) {
        uint8_t rd = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        uint8_t rn = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
        const Operand &op2 = requireOperand(ops, 2, mnemonic);
        if (op2.kind == Operand::Reg) {
            emitDPReg(cond, opcode, s, rn, rd, static_cast<uint8_t>(op2.reg));
        } else {
            emitDPImm(cond, opcode, s, rn, rd, evalImm(op2));
        }
    }
    // `mov`/`mov<cc> rd,op2` - MOV has no first-source-register field.
    void encodeMov(uint8_t cond, const std::vector<Operand> &ops, const std::string &mnemonic) {
        uint8_t rd = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        const Operand &op2 = requireOperand(ops, 1, mnemonic);
        if (op2.kind == Operand::Reg) {
            emitDPReg(cond, 0xD, 0, 0, rd, static_cast<uint8_t>(op2.reg));
        } else {
            emitDPImm(cond, 0xD, 0, 0, rd, evalImm(op2));
        }
    }
    // `mvn rd,rm` - register operand2 only.
    void encodeMvn(const std::vector<Operand> &ops, const std::string &mnemonic) {
        uint8_t rd = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        uint8_t rm = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
        emitDPReg(0xE, 0xF, 0, 0, rd, rm);
    }
    // `and`/`orr`/`eor rd,rn,rm`, register operand2 only.
    void encodeLogical(uint8_t opcode, const std::vector<Operand> &ops, const std::string &mnemonic) {
        uint8_t rd = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        uint8_t rn = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
        uint8_t rm = requireReg(requireOperand(ops, 2, mnemonic), mnemonic);
        emitDPReg(0xE, opcode, 0, rn, rd, rm);
    }
    // `rsb rd,rm,#0` - negate idiom (`rd = 0 - rm`).
    void encodeRsb(const std::vector<Operand> &ops, const std::string &mnemonic) {
        uint8_t rd = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        uint8_t rn = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
        uint32_t imm = evalImm(requireOperand(ops, 2, mnemonic));
        emitDPImm(0xE, 0x3, 0, rn, rd, imm);
    }
    // `cmp rn,rm`/`cmp rn,#imm` - sets flags, Rd unused, S always 1.
    void encodeCmp(const std::vector<Operand> &ops, const std::string &mnemonic) {
        uint8_t rn = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        const Operand &op2 = requireOperand(ops, 1, mnemonic);
        if (op2.kind == Operand::Reg) {
            emitDPReg(0xE, 0xA, 1, rn, 0, static_cast<uint8_t>(op2.reg));
        } else {
            emitDPImm(0xE, 0xA, 1, rn, 0, evalImm(op2));
        }
    }
    // `mul rd,rn,rm`: cond 000000 A(=0) S(=0) Rd 0000 Rs 1001 Rm.
    void encodeMul(const std::vector<Operand> &ops, const std::string &mnemonic) {
        uint8_t rd = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        uint8_t rn = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
        uint8_t rm = requireReg(requireOperand(ops, 2, mnemonic), mnemonic);
        emit32((0xEu << 28) | (static_cast<uint32_t>(rd) << 16) | (static_cast<uint32_t>(rn) << 8) | (0x9u << 4) |
               rm);
    }
    // `umull rdLo,rdHi,rn,rm` (RdHi:RdLo = Rn*Rm, full unsigned 64-bit
    // product): cond 000 opcode(4)=0100 S RdHi RdLo Rs 1001 Rm.
    void encodeUmull(const std::vector<Operand> &ops, const std::string &mnemonic) {
        uint8_t rdLo = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        uint8_t rdHi = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
        uint8_t rn = requireReg(requireOperand(ops, 2, mnemonic), mnemonic);
        uint8_t rm = requireReg(requireOperand(ops, 3, mnemonic), mnemonic);
        emit32((0xEu << 28) | (0x4u << 21) | (static_cast<uint32_t>(rdHi) << 16) |
               (static_cast<uint32_t>(rdLo) << 12) | (static_cast<uint32_t>(rn) << 8) | (0x9u << 4) | rm);
    }
    // `sdiv rd,rn,rm` (`Rd = Rn / Rm`, signed): cond 0111 0001 Rd
    // 1111 Rm 0001 Rn - note Rm/Rn swap fields, per the real SDIV encoding.
    void encodeSdiv(const std::vector<Operand> &ops, const std::string &mnemonic) {
        encodeDiv(0x71u, ops, mnemonic);
    }
    // `udiv rd,rn,rm` - same shape as `sdiv`, bit21 differs (0111 0011).
    void encodeUdiv(const std::vector<Operand> &ops, const std::string &mnemonic) {
        encodeDiv(0x73u, ops, mnemonic);
    }
    void encodeDiv(uint32_t opBits, const std::vector<Operand> &ops, const std::string &mnemonic) {
        uint8_t rd = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        uint8_t rn = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
        uint8_t rm = requireReg(requireOperand(ops, 2, mnemonic), mnemonic);
        emit32((0xEu << 28) | (opBits << 20) | (static_cast<uint32_t>(rd) << 16) | (0xFu << 12) |
               (static_cast<uint32_t>(rm) << 8) | (0x1u << 4) | rn);
    }
    // `mls rd,rn,rm,ra` (`Rd = Ra - Rn*Rm`): cond 0000 0110 Rd Ra Rm 1001 Rn.
    void encodeMls(const std::vector<Operand> &ops, const std::string &mnemonic) {
        uint8_t rd = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        uint8_t rn = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
        uint8_t rm = requireReg(requireOperand(ops, 2, mnemonic), mnemonic);
        uint8_t ra = requireReg(requireOperand(ops, 3, mnemonic), mnemonic);
        emit32((0xEu << 28) | (0x06u << 20) | (static_cast<uint32_t>(rd) << 16) | (static_cast<uint32_t>(ra) << 12) |
               (static_cast<uint32_t>(rm) << 8) | (0x9u << 4) | rn);
    }
    // `lsl`/`lsr rd,rn,op2` (`rd = rn shifted by op2`), encoded as a MOV
    // with a shifted-register operand2 (`shtype`=00 LSL, 01 LSR): register
    // shift amount is cond 000 1101 0 0000 Rd Rs 0 shtype 1 Rm; immediate
    // shift amount is cond 000 1101 0 0000 Rd shamt(5) shtype 0 Rm.
    void encodeShift(uint8_t shtype, const std::vector<Operand> &ops, const std::string &mnemonic) {
        uint8_t rd = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        uint8_t rn = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
        const Operand &amount = requireOperand(ops, 2, mnemonic);
        if (amount.kind == Operand::Reg) {
            uint8_t rs = static_cast<uint8_t>(amount.reg);
            emit32((0xEu << 28) | (0x1Au << 20) | (static_cast<uint32_t>(rd) << 12) |
                   (static_cast<uint32_t>(rs) << 8) | (static_cast<uint32_t>(shtype) << 5) | (1u << 4) | rn);
        } else {
            uint32_t shamt = evalImm(amount);
            if (shamt > 31) fail("'" + mnemonic + "': shift amount out of 0-31 range");
            emit32((0xEu << 28) | (0x1Au << 20) | (static_cast<uint32_t>(rd) << 12) | (shamt << 7) |
                   (static_cast<uint32_t>(shtype) << 5) | rn);
        }
    }
    // `movw`/`movt rd,op2`: cond 0011 000(w)/100(t) imm4 Rd imm12,
    // where imm16 = imm4:imm12.
    void encodeMovWT(bool top, const std::vector<Operand> &ops, const std::string &mnemonic) {
        uint8_t rd = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        const Operand &immOp = requireOperand(ops, 1, mnemonic);
        std::string externalName;
        bool needsBaseShift = false;
        uint16_t imm16 = evalHiLo(immOp, /*hi=*/top, &externalName, &needsBaseShift);
        if (stage_ == 2 && allowExternal_ && immOp.kind == Operand::Mem) {
            uint16_t kind = top ? kArmRelocMovTHi : kArmRelocMovWLo;
            if (needsBaseShift) relocations_.push_back({offset_, "..", kind});
            if (!externalName.empty()) relocations_.push_back({offset_, externalName, kind});
        }
        uint32_t imm4 = (imm16 >> 12) & 0xFu;
        uint32_t imm12 = imm16 & 0xFFFu;
        emit32((0xEu << 28) | ((top ? 0x34u : 0x30u) << 20) | (imm4 << 16) | (static_cast<uint32_t>(rd) << 12) |
               imm12);
    }
    // `ldind addrReg,destReg` / `stind addrReg,srcReg` - LDR/STR with a
    // fixed `[Rn, #0]` offset: cond 01 I(=0) P(=1) U(=1) B(=0) W(=0)
    // L Rn Rd 000000000000.
    void encodeLdindStind(bool isLoad, const std::vector<Operand> &ops, const std::string &mnemonic) {
        uint8_t base = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        uint8_t rt = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
        uint32_t word = (0xEu << 28) | (0x05u << 24) | (0x1u << 23) | (static_cast<uint32_t>(base) << 16) |
                         (static_cast<uint32_t>(rt) << 12);
        if (isLoad) word |= (0x1u << 20);
        emit32(word);
    }
    // `ldindb`/`stindb addrReg,destOrSrcReg` - LDRB/STRB, zero offset:
    // same encoding as encodeLdindStind() with the B bit (bit22) set.
    void encodeLdindStindB(bool isLoad, const std::vector<Operand> &ops, const std::string &mnemonic) {
        uint8_t base = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        uint8_t rt = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
        uint32_t word = (0xEu << 28) | (0x05u << 24) | (0x1u << 23) | (0x1u << 22) |
                         (static_cast<uint32_t>(base) << 16) | (static_cast<uint32_t>(rt) << 12);
        if (isLoad) word |= (0x1u << 20);
        emit32(word);
    }
    // `ldindh`/`stindh`/`ldindsb`/`ldindsh addrReg,destReg` - ARM "extra
    // load/store" family (halfword, signed byte/halfword load; no
    // STRSB/STRSH since a store never needs a sign), zero offset,
    // immediate form: cond 000 P(1) U(1) I(1) W(0) L Rn Rt imm4H(0) 1 S H
    // 1 imm4L(0). `s`/`h` pick LDRH(0,1)/LDRSB(1,0)/LDRSH(1,1)/STRH(0,1).
    void encodeExtraLdSt(bool isLoad, bool s, bool h, const std::vector<Operand> &ops, const std::string &mnemonic) {
        uint8_t base = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        uint8_t rt = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
        uint32_t word = (0xEu << 28) | (0x1u << 24) | (0x1u << 23) | (0x1u << 22) | (static_cast<uint32_t>(base) << 16) |
                         (static_cast<uint32_t>(rt) << 12) | (0x1u << 7) | ((s ? 1u : 0u) << 6) | ((h ? 1u : 0u) << 5) |
                         (0x1u << 4);
        if (isLoad) word |= (0x1u << 20);
        emit32(word);
    }
    // `b`/`bl<cc> label`: cond 101 L offset24, offset24 = (target -
    // (thisInstrAddr+8)) / 4 - the "+8" is the architectural "PC reads as
    // branch address plus 8" quirk.
    void encodeBranch(uint8_t cond, bool link, const std::vector<Operand> &ops, const std::string &mnemonic) {
        const Operand &target = requireOperand(ops, 0, mnemonic);
        if (target.kind != Operand::Mem) fail("'" + mnemonic + "': target must be a label");
        std::string externalName;
        uint32_t targetAddr = evalExpr(target.expr, &externalName);
        // A target defined in this object is already a valid PC-relative
        // offset regardless of link address, so needs no relocation.
        // Only a cross-object target (still undefined here) does; the
        // field is left zero and ArmLinkerFactory.cpp's patch function
        // fills it in once the symbol's linked address is known.
        if (stage_ == 2 && allowExternal_ && !externalName.empty()) {
            relocations_.push_back({offset_, externalName, kArmRelocRel24});
            emit32((static_cast<uint32_t>(cond) << 28) | (0x5u << 25) | ((link ? 1u : 0u) << 24));
            return;
        }
        uint32_t branchPc = orgBase_ + offset_ + 8;
        int32_t rel = (static_cast<int32_t>(targetAddr) - static_cast<int32_t>(branchPc)) / 4;
        if (stage_ == 2 && (rel < -(1 << 23) || rel > (1 << 23) - 1)) fail("branch target out of 24-bit range");
        emit32((static_cast<uint32_t>(cond) << 28) | (0x5u << 25) | ((link ? 1u : 0u) << 24) |
               (static_cast<uint32_t>(rel) & 0x00FFFFFFu));
    }
    // `bx rn`: cond 0001 0010 1111 1111 1111 0001 Rm.
    void encodeBx(const std::vector<Operand> &ops, const std::string &mnemonic) {
        uint8_t rm = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        emit32((0xEu << 28) | 0x012FFF10u | rm);
    }
    // `blx rn` (`BLX (register)`, a near call through a register rather
    // than a label): cond 0001 0010 1111 1111 1111 0011 Rm - same as
    // `bx` but bits 7:4 are 0011, writing the return address into `lr`.
    void encodeBlx(const std::vector<Operand> &ops, const std::string &mnemonic) {
        uint8_t rm = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        emit32((0xEu << 28) | 0x012FFF30u | rm);
    }

    // Not a general A32 encoder: only the mnemonics ArmCodegen.cpp emits.
    void encodeInstr(const std::string &mnemonic, const std::vector<Operand> &ops) {
        if (mnemonic == "add") {
            encodeAddSub(0xE, 0x4, ops, mnemonic);
            return;
        }
        if (mnemonic == "sub") {
            encodeAddSub(0xE, 0x2, ops, mnemonic);
            return;
        }
        // adds/subs set carry/borrow for the following adc/sbc, forming a
        // `long long` ripple-carry add/sub.
        if (mnemonic == "adds") {
            encodeAddSubS(0xE, 0x4, 1, ops, mnemonic);
            return;
        }
        if (mnemonic == "subs") {
            encodeAddSubS(0xE, 0x2, 1, ops, mnemonic);
            return;
        }
        if (mnemonic == "adc") {
            encodeAddSub(0xE, 0x5, ops, mnemonic);
            return;
        }
        if (mnemonic == "sbc") {
            encodeAddSub(0xE, 0x6, ops, mnemonic);
            return;
        }
        if (mnemonic == "rsb") {
            encodeRsb(ops, mnemonic);
            return;
        }
        if (mnemonic == "and") {
            encodeLogical(0x0, ops, mnemonic);
            return;
        }
        if (mnemonic == "orr") {
            encodeLogical(0xC, ops, mnemonic);
            return;
        }
        if (mnemonic == "eor") {
            encodeLogical(0x1, ops, mnemonic);
            return;
        }
        if (mnemonic == "mvn") {
            encodeMvn(ops, mnemonic);
            return;
        }
        if (mnemonic == "cmp") {
            encodeCmp(ops, mnemonic);
            return;
        }
        if (mnemonic == "mul") {
            encodeMul(ops, mnemonic);
            return;
        }
        if (mnemonic == "umull") {
            encodeUmull(ops, mnemonic);
            return;
        }
        if (mnemonic == "sdiv") {
            encodeSdiv(ops, mnemonic);
            return;
        }
        if (mnemonic == "udiv") {
            encodeUdiv(ops, mnemonic);
            return;
        }
        if (mnemonic == "mls") {
            encodeMls(ops, mnemonic);
            return;
        }
        if (mnemonic == "lsl") {
            encodeShift(0x0, ops, mnemonic);
            return;
        }
        if (mnemonic == "lsr") {
            encodeShift(0x1, ops, mnemonic);
            return;
        }
        if (mnemonic == "asr") {
            encodeShift(0x2, ops, mnemonic);
            return;
        }
        if (mnemonic == "movw") {
            encodeMovWT(false, ops, mnemonic);
            return;
        }
        if (mnemonic == "movt") {
            encodeMovWT(true, ops, mnemonic);
            return;
        }
        if (mnemonic == "ldind") {
            encodeLdindStind(true, ops, mnemonic);
            return;
        }
        if (mnemonic == "stind") {
            encodeLdindStind(false, ops, mnemonic);
            return;
        }
        if (mnemonic == "ldindb") {
            encodeLdindStindB(true, ops, mnemonic);
            return;
        }
        if (mnemonic == "stindb") {
            encodeLdindStindB(false, ops, mnemonic);
            return;
        }
        if (mnemonic == "ldindsb") {
            encodeExtraLdSt(true, /*s=*/true, /*h=*/false, ops, mnemonic);
            return;
        }
        if (mnemonic == "ldindh") {
            encodeExtraLdSt(true, /*s=*/false, /*h=*/true, ops, mnemonic);
            return;
        }
        if (mnemonic == "stindh") {
            encodeExtraLdSt(false, /*s=*/false, /*h=*/true, ops, mnemonic);
            return;
        }
        if (mnemonic == "ldindsh") {
            encodeExtraLdSt(true, /*s=*/true, /*h=*/true, ops, mnemonic);
            return;
        }
        if (mnemonic == "bx") {
            encodeBx(ops, mnemonic);
            return;
        }
        if (mnemonic == "blx") {
            encodeBlx(ops, mnemonic);
            return;
        }
        if (mnemonic == "bl") {
            encodeBranch(0xE, /*link=*/true, ops, mnemonic);
            return;
        }
        if (mnemonic.size() >= 1 && mnemonic[0] == 'b' && mnemonic != "bl" && mnemonic != "bx" && mnemonic != "blx") {
            encodeBranch(condOf(mnemonic.substr(1)), /*link=*/false, ops, mnemonic);
            return;
        }
        if (mnemonic.size() >= 3 && mnemonic.substr(0, 3) == "mov") {
            encodeMov(condOf(mnemonic.substr(3)), ops, mnemonic);
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

// Builds the format-neutral ObjectFile from a completed object-mode
// Assembler.
ObjectFile buildObjectFile(const Assembler &a) {
    ObjectFile obj;
    obj.machineType = kArmCoffMachine;
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

std::string assembleArm(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/false);
    assembler.run(lines);
    const auto &bytes = assembler.bytes();
    return std::string(bytes.begin(), bytes.end());
}

ObjectFile assembleArmObject(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/true);
    assembler.run(lines);
    return buildObjectFile(assembler);
}

}  // namespace rcc
