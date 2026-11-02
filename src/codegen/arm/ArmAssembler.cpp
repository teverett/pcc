#include "ArmAssembler.h"

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

[[noreturn]] void fail(const std::string &msg) { throw std::runtime_error("ArmAssembler: " + msg); }

// --- Parsing, via the one shared grammar/PccAsm.g4 -----------------------
// Same Syllable/Group shape as every other backend's own assembler -
// see M68kAssembler.cpp's own comment.
enum class SylKind { Number, Symbol, Group };
struct Syllable {
    char sign = 0;
    SylKind kind;
    std::string text;
    std::vector<Syllable> group;
};

// One operand, classified from a bare expression - see
// ArmAssembler.h's own comment on why there's no real `#imm`/
// `[Rn, #imm]` ARM syntax here. `Reg` comes from recognizing a bare
// register name; `Imm` from a bare (optionally signed) number;
// anything else (a label, or `label+N`) is `Mem` - an absolute
// address, resolved to the label's own full 32-bit value and then
// (for `movw`/`movt` specifically - see ArmAssembler.cpp's own
// encodeInstr()) split into its lower/upper half.
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

// This dialect's own fixed register numbering (not a real ARM
// register-name table in full - just the handful ArmCodegen.cpp ever
// emits - see ArmCodegen.h's own header comment on each register's
// role): `r0`-`r3`=0-3, `sp`=13, `lr`=14 - the real ARM register
// numbers those names carry in every actual ARM assembler too, so
// the encoded bit patterns below match genuine A32 object code
// exactly, even though this dialect only ever names this one subset
// of the 16 GPRs. Returns -1 if `name` isn't one of them.
int regNumber(const std::string &lower) {
    static const std::unordered_map<std::string, int> names = {
        {"r0", 0}, {"r1", 1}, {"r2", 2}, {"r3", 3}, {"sp", 13}, {"lr", 14},
    };
    auto it = names.find(lower);
    return it == names.end() ? -1 : it->second;
}

Operand buildOperand(PccAsmParser::OperandContext *ctx) {
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
    if (mnemonic == "dw") {
        line.kind = Line::Dw;
        for (auto *op : instr->operand()) {
            if (op->LBRACKET()) fail("'dw' operand can't be bracketed");
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

// --- Two-pass assembly ----------------------------------------------------
class Assembler {
   public:
    void run(const std::vector<Line> &lines) {
        stage_ = 1;
        pass(lines);
        stage_ = 2;
        pass(lines);
    }

    const std::vector<uint8_t> &bytes() const { return bytes_; }

   private:
    void pass(const std::vector<Line> &lines) {
        bytes_.clear();
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
            case Line::Instr:
                encodeInstr(line.mnemonic, line.operands);
                return;
        }
    }

    uint32_t evalExpr(const std::vector<Syllable> &syls) {
        uint32_t total = 0;
        for (const auto &syl : syls) {
            uint32_t v = resolveSyllable(syl);
            if (syl.sign == '-') {
                total -= v;
            } else {
                total += v;
            }
        }
        return total;
    }

    uint32_t resolveSyllable(const Syllable &syl) {
        if (syl.kind == SylKind::Group) return evalExpr(syl.group);
        if (syl.kind == SylKind::Number) {
            const std::string &t = syl.text;
            if (t.size() > 2 && t[0] == '0' && (t[1] == 'x' || t[1] == 'X')) {
                return static_cast<uint32_t>(std::stoul(t.substr(2), nullptr, 16));
            }
            return static_cast<uint32_t>(std::stoul(t));
        }
        auto it = labels_.find(syl.text);
        if (it != labels_.end()) return it->second;
        if (stage_ == 2) fail("undefined symbol '" + syl.text + "'");
        return 0;
    }

    // --- word emission (little-endian, real ARM memory order) ----------
    void emit32(uint32_t v) {
        bytes_.push_back(static_cast<uint8_t>(v & 0xFF));
        bytes_.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        bytes_.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
        bytes_.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
        offset_ += 4;
    }

    // `op`'s own exprList already carries a plain (optionally signed)
    // decimal/hex value (e.g. `mov r0,-4`) - this dialect's immediate
    // operands are always used in a context ArmCodegen.cpp itself
    // already range-checked (an 8-bit data-processing immediate, a
    // 5-bit shift amount), so no range check is repeated here.
    uint32_t evalImm(const Operand &op) {
        if (op.kind == Operand::Reg) fail("expected an immediate or label operand, not a register");
        return evalExpr(op.expr);
    }

    // `movw`/`movt`'s own immediate operand: a plain number is used
    // exactly as ArmCodegen.cpp already computed it (that half of a
    // literal it split itself - see IntLit's own codegen); a label
    // (`Mem`) instead has its own full resolved address split into
    // its lower (`movw`) or upper (`movt`) 16 bits automatically here -
    // see ArmAssembler.h's own header comment on why this dialect
    // needs no `:lower16:`/`:upper16:` syntax at all.
    uint16_t evalHiLo(const Operand &op, bool hi) {
        if (op.kind == Operand::Reg) fail("expected an immediate or label operand, not a register");
        uint32_t full = evalExpr(op.expr);
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
    // Every A32 instruction's own top 4 bits are its condition field -
    // this dialect spells the condition as a mnemonic suffix (`moveq`,
    // `beq`, ...), same idea as real UAL syntax, just without the
    // `IT`-block machinery Thumb would otherwise need (this dialect
    // only ever emits full 32-bit A32 encodings - see ArmAssembler.h's
    // own header comment).
    static uint8_t condOf(const std::string &suffix) {
        static const std::unordered_map<std::string, uint8_t> conds = {
            {"", 0xE}, {"eq", 0x0}, {"ne", 0x1}, {"lt", 0xB}, {"ge", 0xA}, {"le", 0xD}, {"gt", 0xC},
        };
        auto it = conds.find(suffix);
        if (it == conds.end()) fail("unsupported condition suffix '" + suffix + "'");
        return it->second;
    }

    // --- A32 instruction formats -----------------------------------------
    // Data-processing, register operand2 (shift_imm=0, shift_type=0,
    // bit4=0 - i.e. operand2 is simply `Rm`, unshifted): cond(4) 00
    // 0 opcode(4) S(1) Rn(4) Rd(4) 00000000 Rm(4).
    void emitDPReg(uint8_t cond, uint8_t opcode, uint8_t s, uint8_t rn, uint8_t rd, uint8_t rm) {
        emit32((static_cast<uint32_t>(cond) << 28) | (static_cast<uint32_t>(opcode) << 21) |
               (static_cast<uint32_t>(s) << 20) | (static_cast<uint32_t>(rn) << 16) |
               (static_cast<uint32_t>(rd) << 12) | rm);
    }
    // Data-processing, immediate operand2 (rotate=0, i.e. operand2 is
    // simply `imm8` with no rotation - every immediate this dialect
    // ever emits already fits 0-255 unrotated, see ArmCodegen.cpp):
    // cond(4) 00 1 opcode(4) S(1) Rn(4) Rd(4) 0000 imm8(8).
    void emitDPImm(uint8_t cond, uint8_t opcode, uint8_t s, uint8_t rn, uint8_t rd, uint32_t imm8) {
        if (imm8 > 0xFF) fail("data-processing immediate out of 8-bit range");
        emit32((static_cast<uint32_t>(cond) << 28) | (1u << 25) | (static_cast<uint32_t>(opcode) << 21) |
               (static_cast<uint32_t>(s) << 20) | (static_cast<uint32_t>(rn) << 16) |
               (static_cast<uint32_t>(rd) << 12) | imm8);
    }
    // `add`/`sub`: dialect order `mnemonic rd,rn,op2` where op2 is
    // either a register (plain `ADD`/`SUB`) or an immediate (`ADD`/
    // `SUB` with I=1) - ArmCodegen.cpp only ever uses one or the
    // other depending on context, never needing both forms of the
    // same expression.
    void encodeAddSub(uint8_t cond, uint8_t opcode, const std::vector<Operand> &ops, const std::string &mnemonic) {
        uint8_t rd = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        uint8_t rn = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
        const Operand &op2 = requireOperand(ops, 2, mnemonic);
        if (op2.kind == Operand::Reg) {
            emitDPReg(cond, opcode, 0, rn, rd, static_cast<uint8_t>(op2.reg));
        } else {
            emitDPImm(cond, opcode, 0, rn, rd, evalImm(op2));
        }
    }
    // `mov`/`mov<cc>`: dialect order `mnemonic rd,op2`, Rn unused
    // (opcode MOV has no first-source-register field at all).
    void encodeMov(uint8_t cond, const std::vector<Operand> &ops, const std::string &mnemonic) {
        uint8_t rd = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        const Operand &op2 = requireOperand(ops, 1, mnemonic);
        if (op2.kind == Operand::Reg) {
            emitDPReg(cond, 0xD, 0, 0, rd, static_cast<uint8_t>(op2.reg));
        } else {
            emitDPImm(cond, 0xD, 0, 0, rd, evalImm(op2));
        }
    }
    // `mvn rd,rm` - register operand2 only (ArmCodegen.cpp's own `~`
    // always has a register operand).
    void encodeMvn(const std::vector<Operand> &ops, const std::string &mnemonic) {
        uint8_t rd = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        uint8_t rm = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
        emitDPReg(0xE, 0xF, 0, 0, rd, rm);
    }
    // `and`/`orr`/`eor` - dialect order `mnemonic rd,rn,rm`, register
    // operand2 only.
    void encodeLogical(uint8_t opcode, const std::vector<Operand> &ops, const std::string &mnemonic) {
        uint8_t rd = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        uint8_t rn = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
        uint8_t rm = requireReg(requireOperand(ops, 2, mnemonic), mnemonic);
        emitDPReg(0xE, opcode, 0, rn, rd, rm);
    }
    // `rsb rd,rm,#0` - this dialect's own negate idiom (`rd = 0 -
    // rm`), immediate operand2 only (always literally `0`).
    void encodeRsb(const std::vector<Operand> &ops, const std::string &mnemonic) {
        uint8_t rd = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        uint8_t rn = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
        uint32_t imm = evalImm(requireOperand(ops, 2, mnemonic));
        emitDPImm(0xE, 0x3, 0, rn, rd, imm);
    }
    // `cmp rn,rm` - sets flags, no destination register (Rd field is
    // unused/zero), S is architecturally always 1. Register operand2
    // only (ArmCodegen.cpp's own comparisons always compare two
    // already-evaluated values, never a bare immediate).
    void encodeCmp(const std::vector<Operand> &ops, const std::string &mnemonic) {
        uint8_t rn = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        const Operand &op2 = requireOperand(ops, 1, mnemonic);
        if (op2.kind == Operand::Reg) {
            emitDPReg(0xE, 0xA, 1, rn, 0, static_cast<uint8_t>(op2.reg));
        } else {
            emitDPImm(0xE, 0xA, 1, rn, 0, evalImm(op2));
        }
    }
    // `mul rd,rn,rm` - dialect order matches the real instruction's
    // own field order directly (`Rd = Rm_field * Rs_field`,
    // multiplication being commutative, so which dialect operand maps
    // to which hardware field doesn't matter for correctness): cond
    // 000000 A(=0) S(=0) Rd 0000 Rs 1001 Rm.
    void encodeMul(const std::vector<Operand> &ops, const std::string &mnemonic) {
        uint8_t rd = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        uint8_t rn = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
        uint8_t rm = requireReg(requireOperand(ops, 2, mnemonic), mnemonic);
        emit32((0xEu << 28) | (static_cast<uint32_t>(rd) << 16) | (static_cast<uint32_t>(rn) << 8) | (0x9u << 4) |
               rm);
    }
    // `sdiv rd,rn,rm` (`Rd = Rn / Rm`, signed): cond 0111 0001 Rd
    // 1111 Rm 0001 Rn - note the hardware field order (`Rm` sits
    // where `Rn` usually would, and vice versa) is the real ARMv7-A
    // SDIV encoding, not a dialect convention.
    void encodeSdiv(const std::vector<Operand> &ops, const std::string &mnemonic) {
        uint8_t rd = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        uint8_t rn = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
        uint8_t rm = requireReg(requireOperand(ops, 2, mnemonic), mnemonic);
        emit32((0xEu << 28) | (0x71u << 20) | (static_cast<uint32_t>(rd) << 16) | (0xFu << 12) |
               (static_cast<uint32_t>(rm) << 8) | (0x1u << 4) | rn);
    }
    // `mls rd,rn,rm,ra` (`Rd = Ra - Rn*Rm`): cond 0000 0110 Rd Ra Rm
    // 1001 Rn.
    void encodeMls(const std::vector<Operand> &ops, const std::string &mnemonic) {
        uint8_t rd = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        uint8_t rn = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
        uint8_t rm = requireReg(requireOperand(ops, 2, mnemonic), mnemonic);
        uint8_t ra = requireReg(requireOperand(ops, 3, mnemonic), mnemonic);
        emit32((0xEu << 28) | (0x06u << 20) | (static_cast<uint32_t>(rd) << 16) | (static_cast<uint32_t>(ra) << 12) |
               (static_cast<uint32_t>(rm) << 8) | (0x9u << 4) | rn);
    }
    // `lsl`/`lsr rd,rn,op2` (dialect: `rd = rn shifted by op2`) - op2
    // either a register (real ARM "register-shifted-register"
    // operand2 - the shift *amount* register sits in the `Rs` field,
    // the value-to-shift register sits where `Rm` normally would:
    // cond 000 1101 0 0000 Rd Rs 0 shtype 1 Rm) or an immediate shift
    // amount (real ARM "immediate-shifted-register" operand2: cond
    // 000 1101 0 0000 Rd shamt(5) shtype 0 Rm) - both forms are a
    // `MOV` with a shifted-register operand2 underneath (the real UAL
    // `LSL`/`LSR` mnemonics are themselves just aliases for exactly
    // this), `shtype`=00 for LSL, 01 for LSR.
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
    // where the full imm16 = imm4:imm12 - a plain number is used
    // exactly as ArmCodegen.cpp already computed it; a label (`Mem`)
    // has its own full resolved address auto-split into its
    // lower/upper half (see evalHiLo()'s own comment).
    void encodeMovWT(bool top, const std::vector<Operand> &ops, const std::string &mnemonic) {
        uint8_t rd = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        uint16_t imm16 = evalHiLo(requireOperand(ops, 1, mnemonic), /*hi=*/top);
        uint32_t imm4 = (imm16 >> 12) & 0xFu;
        uint32_t imm12 = imm16 & 0xFFFu;
        emit32((0xEu << 28) | ((top ? 0x34u : 0x30u) << 20) | (imm4 << 16) | (static_cast<uint32_t>(rd) << 12) |
               imm12);
    }
    // Dedicated zero-offset load/store - see ArmCodegen.h's own
    // header comment: `ldind addrReg,destReg` / `stind
    // addrReg,srcReg`, real `LDR`/`STR` underneath with a literal
    // `[Rn, #0]` offset: cond 01 I(=0) P(=1) U(=1) B(=0) W(=0)
    // L Rn Rd 000000000000.
    void encodeLdindStind(bool isLoad, const std::vector<Operand> &ops, const std::string &mnemonic) {
        uint8_t base = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        uint8_t rt = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
        uint32_t word = (0xEu << 28) | (0x05u << 24) | (0x1u << 23) | (static_cast<uint32_t>(base) << 16) |
                         (static_cast<uint32_t>(rt) << 12);
        if (isLoad) word |= (0x1u << 20);
        emit32(word);
    }
    // `b`/`bl<cc> label`: cond 101 L offset24, offset24 = (target -
    // (thisInstrAddr+8)) / 4 - real A32 PC-relative branch semantics
    // (the "+8" is the real architectural "PC reads as the address of
    // the branch instruction plus 8" quirk, not a pipeline detail this
    // assembler invented).
    void encodeBranch(uint8_t cond, bool link, const std::vector<Operand> &ops, const std::string &mnemonic) {
        const Operand &target = requireOperand(ops, 0, mnemonic);
        if (target.kind != Operand::Mem) fail("'" + mnemonic + "': target must be a label");
        uint32_t targetAddr = evalExpr(target.expr);
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

    // The full instruction vocabulary ../ArmCodegen.cpp ever emits -
    // see ArmAssembler.h's own header comment. Not a general A32
    // encoder: exactly these mnemonic/operand-shape combinations.
    void encodeInstr(const std::string &mnemonic, const std::vector<Operand> &ops) {
        if (mnemonic == "add") {
            encodeAddSub(0xE, 0x4, ops, mnemonic);
            return;
        }
        if (mnemonic == "sub") {
            encodeAddSub(0xE, 0x2, ops, mnemonic);
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
        if (mnemonic == "sdiv") {
            encodeSdiv(ops, mnemonic);
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
        if (mnemonic == "bx") {
            encodeBx(ops, mnemonic);
            return;
        }
        if (mnemonic == "bl") {
            encodeBranch(0xE, /*link=*/true, ops, mnemonic);
            return;
        }
        if (mnemonic.size() >= 1 && mnemonic[0] == 'b' && mnemonic != "bl" && mnemonic != "bx") {
            // `b`/`beq`/`bne` - the condition suffix is whatever
            // follows the leading 'b'.
            encodeBranch(condOf(mnemonic.substr(1)), /*link=*/false, ops, mnemonic);
            return;
        }
        if (mnemonic.size() >= 3 && mnemonic.substr(0, 3) == "mov") {
            // `mov`/`moveq`/`movne`/`movlt`/`movge`/`movle`/`movgt` -
            // the condition suffix is whatever follows "mov".
            encodeMov(condOf(mnemonic.substr(3)), ops, mnemonic);
            return;
        }
        fail("unsupported instruction '" + mnemonic + "'");
    }

    std::vector<uint8_t> bytes_;
    std::unordered_map<std::string, uint32_t> labels_;
    uint32_t offset_ = 0;
    uint32_t orgBase_ = 0;
    int stage_ = 1;
};

}  // namespace

std::string assembleArm(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler;
    assembler.run(lines);
    const auto &bytes = assembler.bytes();
    return std::string(bytes.begin(), bytes.end());
}

}  // namespace pcc
