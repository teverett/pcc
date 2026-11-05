#include "PowerPCAssembler.h"

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

[[noreturn]] void fail(const std::string &msg) { throw std::runtime_error("PowerPCAssembler: " + msg); }

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
// PowerPCAssembler.h's own comment on why there's no real
// `0(r4)`/`%hi()`/`%lo()` PowerPC syntax here. `Reg` comes from
// recognizing a bare register name; `Imm` from a bare (optionally
// signed) number; anything else (a label, or `label+N`) is `Mem` - an
// absolute address, resolved to the label's own full 32-bit value and
// then (for `lis`/`ori` specifically - see encodeInstr()) split into
// its upper/lower half.
struct Operand {
    enum Kind { Reg, Imm, Mem } kind = Imm;
    int reg = -1;
    std::vector<Syllable> expr;  // Imm/Mem
};

struct Line {
    std::vector<std::string> labels;
    enum Kind { None, Equate, Org, Db, Dh, Dw, Instr } kind = None;
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

// This dialect's own fixed register numbering (not a real PowerPC ABI
// register-name table in full - just the handful PowerPCCodegen.cpp
// ever emits - see PowerPCCodegen.h's own header comment on each
// register's role): `r0`=0, `sp`=1, `t0`=3, `t1`=4, `t2`=5, `fp`=31 -
// the real PowerPC register numbers those names carry in every actual
// PowerPC assembler too (`r31` is also real-world PowerPC's own usual
// frame-pointer choice when a function needs one), so the encoded bit
// patterns below match genuine PowerPC object code exactly, even
// though this dialect only ever names this one subset of the 32 GPRs.
// Returns -1 if `name` isn't one of them.
int regNumber(const std::string &lower) {
    static const std::unordered_map<std::string, int> names = {
        {"r0", 0}, {"sp", 1}, {"t0", 3}, {"t1", 4}, {"t2", 5}, {"t3", 6},
        {"t4", 7}, {"t5", 8}, {"t6", 9}, {"fp", 31},
    };
    auto it = names.find(lower);
    return it == names.end() ? -1 : it->second;
}

Operand buildOperand(PccAsmParser::OperandContext *ctx) {
    if (ctx->LBRACKET()) {
        fail(
            "bracketed memory operands aren't part of this target's dialect - PowerPC memory access always goes "
            "through the dedicated 'ldind'/'stind' mnemonics instead (see PowerPCCodegen.h)");
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

// One pending fixup `assemblePowerPCObject()` (see
// PowerPCAssembler.h's own header comment) turns into a real
// ObjRelocation once assembly is done - `offset` into `bytes()`,
// `symbolName` the reference that couldn't be resolved locally (or
// the reserved pseudo-symbol ".." for a same-file address's own
// needed base-shift), `kind` one of kPowerPCRelocHi/Lo/Branch.
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
                // Each word's own expression goes through the exact same
                // external-name/base-shift tracking as `lis`/`ori`'s own
                // Hi/Lo immediate (see encodeInstr()) - a bare symbol here
                // (an array initializer's address-of-function/global
                // element) resolves and relocates as a plain additive
                // 32-bit word (kPowerPCRelocAbs32), rather than the split
                // Hi/Lo pair an instruction's own immediate field needs.
                for (const auto &e : line.exprList) {
                    std::string externalName;
                    bool needsBaseShift = false;
                    uint32_t v = evalExpr(e, &externalName, &needsBaseShift);
                    if (stage_ == 2 && allowExternal_) {
                        if (needsBaseShift) relocations_.push_back({offset_, "..", kPowerPCRelocAbs32});
                        if (!externalName.empty())
                            relocations_.push_back({offset_, externalName, kPowerPCRelocAbs32});
                    }
                    emit32(v);
                }
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

    // `externalName`/`needsBaseShift` mirror ../m68k/M68kAssembler.cpp's
    // own `evalExpr` exactly - see PowerPCAssembler.h's own header
    // comment on why *any* local `lis`/`ori` address reference, not
    // just an external one, needs a relocation here once
    // `allowExternal_` is set (a `b`/`bl` branch target never sets
    // `needsBaseShift` at all - its own caller never passes that
    // out-param, since a same-object branch displacement needs no
    // relocation regardless of where this object ends up linked).
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

    // --- word emission -------------------------------------------------
    void emit8(uint8_t v) {
        bytes_.push_back(v);
        offset_ += 1;
    }

    void emit16(uint16_t v) {
        bytes_.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        bytes_.push_back(static_cast<uint8_t>(v & 0xFF));
        offset_ += 2;
    }

    void emit32(uint32_t v) {
        bytes_.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
        bytes_.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
        bytes_.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        bytes_.push_back(static_cast<uint8_t>(v & 0xFF));
        offset_ += 4;
    }

    static uint16_t imm16Of(int32_t v) {
        if (v < -32768 || v > 65535) fail("immediate out of 16-bit range");
        return static_cast<uint16_t>(v & 0xFFFF);
    }

    // The signed/unsigned decimal `op`'s own exprList already carries
    // (e.g. `li t0,-4`), masked to 16 bits.
    uint16_t evalImm(const Operand &op) {
        if (op.kind == Operand::Reg) fail("expected an immediate or label operand, not a register");
        int32_t v = static_cast<int32_t>(evalExpr(op.expr));
        return imm16Of(v);
    }

    // `lis`/`ori`'s own immediate operand: a plain number is used
    // exactly as PowerPCCodegen.cpp already computed it (that half of
    // a literal it split itself - see IntLit's own codegen); a label
    // (`Mem`) instead has its own full resolved address split into
    // its upper (`lis`) or lower (`ori`) 16 bits automatically here -
    // see PowerPCAssembler.h's own header comment on why this dialect
    // needs no `%hi`/`%lo` syntax at all. `externalName`/
    // `needsBaseShift` just thread through to `evalExpr()` - only a
    // `Mem` operand (a real label reference) can ever set either one.
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

    // --- PowerPC instruction formats ------------------------------------
    // D-form, direct field order (arithmetic: addi/addis/li/lis) -
    // opcode(6) rD/rT(5) rA(5) imm16 - the encoded field order matches
    // the dialect's own mnemonic operand order (dest first).
    void emitDArith(uint8_t opcode, uint8_t rT, uint8_t rA, uint16_t imm) {
        emit32((static_cast<uint32_t>(opcode) << 26) | (static_cast<uint32_t>(rT) << 21) |
               (static_cast<uint32_t>(rA) << 16) | imm);
    }
    // D-form, swapped field order (the logical-immediate family -
    // `ori` is the only one this dialect ever emits) - opcode(6)
    // rS(5) rA(5) imm16 - real PowerPC's own well-known asymmetry: the
    // mnemonic's own first (destination) operand is encoded in the
    // *second* register field, not the first, unlike every arithmetic
    // D-form instruction above.
    void emitDLogical(uint8_t opcode, uint8_t rADest, uint8_t rSSrc, uint16_t imm) {
        emit32((static_cast<uint32_t>(opcode) << 26) | (static_cast<uint32_t>(rSSrc) << 21) |
               (static_cast<uint32_t>(rADest) << 16) | imm);
    }
    // D-form compare: opcode(6) BF(3)=0 /(1)=0 L(1)=0 rA(5) rest(16) -
    // `cmpw`/`cmpwi` always compare word-sized signed operands in crf0
    // here, the only condition register field this dialect ever uses.
    void emitDCompareReg(uint8_t opcode, uint8_t rA, uint8_t rB) {
        emit32((static_cast<uint32_t>(opcode) << 26) | (static_cast<uint32_t>(rA) << 16) |
               (static_cast<uint32_t>(rB) << 11));
    }
    void emitDCompareImm(uint8_t opcode, uint8_t rA, uint16_t imm) {
        emit32((static_cast<uint32_t>(opcode) << 26) | (static_cast<uint32_t>(rA) << 16) | imm);
    }
    // X-form, direct field order (arithmetic: add/subf/mullw/divw/neg)
    // - opcode(6)=31 rD/rT(5) rA(5) rB(5) XO(10) Rc(1) - same "mnemonic
    // order matches field order" convention as emitDArith() above.
    void emitXArith(uint8_t rT, uint8_t rA, uint8_t rB, uint16_t xo) {
        emit32((31u << 26) | (static_cast<uint32_t>(rT) << 21) | (static_cast<uint32_t>(rA) << 16) |
               (static_cast<uint32_t>(rB) << 11) | (static_cast<uint32_t>(xo) << 1));
    }
    // X-form, swapped field order (the logical family - and/or/xor/
    // nor/slw/srw) - opcode(6)=31 rS(5) rA(5) rB(5) XO(10) Rc(1) - same
    // real-hardware asymmetry as emitDLogical() above: the mnemonic's
    // own destination operand lands in the *second* register field.
    void emitXLogical(uint8_t rADest, uint8_t rSSrc, uint8_t rB, uint16_t xo) {
        emit32((31u << 26) | (static_cast<uint32_t>(rSSrc) << 21) | (static_cast<uint32_t>(rADest) << 16) |
               (static_cast<uint32_t>(rB) << 11) | (static_cast<uint32_t>(xo) << 1));
    }
    // XFX-form (mflr/mtlr - the only two SPR-move instructions this
    // dialect ever emits, always against SPR 8, the link register).
    void emitMoveLr(uint32_t base, uint8_t reg) { emit32(base | (static_cast<uint32_t>(reg) << 21)); }

    // Unconditional branch (b/bl) - I-form: opcode(6)=18 LI(24) AA(1)=0
    // LK(1) - `rel` is the byte displacement from this instruction's
    // own address to the target, real PowerPC PC-relative semantics
    // (no branch-delay slot to account for, unlike MIPS - see
    // PowerPCAssembler.h's own header comment).
    void emitBranch(uint32_t rel, bool link) {
        if (stage_ == 2 && (static_cast<int32_t>(rel) < -(1 << 25) || static_cast<int32_t>(rel) >= (1 << 25))) {
            fail("branch target out of 24-bit range");
        }
        emit32((18u << 26) | (rel & 0x03FFFFFCu) | (link ? 1u : 0u));
    }
    // Conditional branch (beq/bne/blt/bgt/ble/bge, all against crf0) -
    // B-form: opcode(6)=16 BO(5) BI(5) BD(14) AA(1)=0 LK(1)=0. The
    // range check only applies in stage 2 - see Assembler::run()'s own
    // two-pass comment: a forward reference resolves to a placeholder
    // 0 in stage 1 (see resolveSyllable()), which would otherwise look
    // like a wildly out-of-range displacement purely as an artifact of
    // not having the real address yet.
    void emitCondBranch(uint8_t bo, uint8_t bi, uint32_t rel) {
        int32_t srel = static_cast<int32_t>(rel);
        if (stage_ == 2 && (srel < -32768 || srel > 32767)) fail("conditional branch target out of 16-bit range");
        emit32((16u << 26) | (static_cast<uint32_t>(bo) << 21) | (static_cast<uint32_t>(bi) << 16) |
               (rel & 0xFFFCu));
    }

    void encodeInstr(const std::string &mnemonic, const std::vector<Operand> &ops) {
        // --- immediate load/arith (D-form, direct) ---
        // `li` never takes a label operand in this dialect (only
        // PowerPCCodegen.cpp's own pre-split literal halves and small
        // boolean/comparison constants - see its own `genExpr()`), so
        // it never needs a relocation here.
        if (mnemonic == "li") {
            uint8_t rT = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            uint16_t imm = evalHiLo(requireOperand(ops, 1, mnemonic), /*hi=*/false);
            emitDArith(14, rT, 0, imm);
            return;
        }
        // `lis` - the upper-half half of a `lis`/`ori` address-load
        // pair (see loadAddress() in ../PowerPCCodegen.cpp): a `Mem`
        // operand (a real label) needs a relocation in object mode -
        // either against ".." (this object's own base, if the label
        // is resolvable locally) or against the label's own name (if
        // it's undefined here, deferred to the linker) - see
        // PowerPCAssembler.h's own `kPowerPCRelocHi` comment. A plain
        // `Imm` operand (PowerPCCodegen.cpp's own already-split
        // literal half) never needs one.
        if (mnemonic == "lis") {
            uint8_t rT = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            const Operand &immOp = requireOperand(ops, 1, mnemonic);
            std::string externalName;
            bool needsBaseShift = false;
            uint16_t imm = evalHiLo(immOp, /*hi=*/true, &externalName, &needsBaseShift);
            if (stage_ == 2 && allowExternal_ && immOp.kind == Operand::Mem) {
                if (needsBaseShift) relocations_.push_back({offset_, "..", kPowerPCRelocHi});
                if (!externalName.empty()) relocations_.push_back({offset_, externalName, kPowerPCRelocHi});
            }
            emitDArith(15, rT, 0, imm);
            return;
        }
        if (mnemonic == "addi") {
            uint8_t rT = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            uint8_t rA = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
            uint16_t imm = evalImm(requireOperand(ops, 2, mnemonic));
            emitDArith(14, rT, rA, imm);
            return;
        }
        // --- immediate load/arith (D-form, swapped) ---
        // `ori` - the lower-half of the same `lis`/`ori` pair above;
        // same relocation treatment, `kPowerPCRelocLo` instead of
        // `kPowerPCRelocHi` - see PowerPCLinkerFactory.cpp's own paired
        // Hi/Lo patch functions for how the two sites' own carry gets
        // computed correctly.
        if (mnemonic == "ori") {
            uint8_t rADest = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            uint8_t rSSrc = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
            const Operand &immOp = requireOperand(ops, 2, mnemonic);
            std::string externalName;
            bool needsBaseShift = false;
            uint16_t imm = evalHiLo(immOp, /*hi=*/false, &externalName, &needsBaseShift);
            if (stage_ == 2 && allowExternal_ && immOp.kind == Operand::Mem) {
                if (needsBaseShift) relocations_.push_back({offset_, "..", kPowerPCRelocLo});
                if (!externalName.empty()) relocations_.push_back({offset_, externalName, kPowerPCRelocLo});
            }
            emitDLogical(24, rADest, rSSrc, imm);
            return;
        }
        // --- compare ---
        if (mnemonic == "cmpw") {
            uint8_t rA = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            uint8_t rB = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
            emitDCompareReg(31, rA, rB);
            return;
        }
        if (mnemonic == "cmpwi") {
            uint8_t rA = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            uint16_t imm = evalImm(requireOperand(ops, 1, mnemonic));
            emitDCompareImm(11, rA, imm);
            return;
        }
        // cmplw - the unsigned counterpart to cmpw, same X-form shape
        // (opcode 31, crfD/L both 0) with XO=32 instead of cmpw's own
        // XO=0 - signed vs. unsigned `long long` comparison (see
        // PowerPCCodegen.cpp's own genWideCompare()) needs both.
        if (mnemonic == "cmplw") {
            uint8_t rA = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            uint8_t rB = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
            emit32((31u << 26) | (static_cast<uint32_t>(rA) << 16) | (static_cast<uint32_t>(rB) << 11) | (32u << 1));
            return;
        }
        // --- shift-by-constant (slwi - a convenience alias for the
        // literal fixed-count left shift PowerPCCodegen.cpp's own
        // array-index scaling needs, encoded as the real `rlwinm`
        // special case GNU/real assemblers use for it: rD = rS << n,
        // n in [0,31]) ---
        if (mnemonic == "slwi") {
            uint8_t rADest = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            uint8_t rSSrc = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
            uint8_t n = static_cast<uint8_t>(evalImm(requireOperand(ops, 2, mnemonic)) & 0x1F);
            // rlwinm rA,rS,SH,MB,ME with SH=n, MB=0, ME=31-n.
            uint8_t sh = n;
            uint8_t mb = 0;
            uint8_t me = static_cast<uint8_t>(31 - n);
            emit32((21u << 26) | (static_cast<uint32_t>(rSSrc) << 21) | (static_cast<uint32_t>(rADest) << 16) |
                   (static_cast<uint32_t>(sh) << 11) | (static_cast<uint32_t>(mb) << 6) |
                   (static_cast<uint32_t>(me) << 1));
            return;
        }
        // srwi rA,rS,n - logical right shift by an immediate count
        // (the `rlwinm` special case for SH=32-n, MB=n, ME=31, the
        // real GNU/standard assembler alias) - a narrower-than-
        // native-int Var/Slot's own unsigned narrowing and the
        // `long long` divide/shift algorithms both need this
        // alongside slwi/srawi (see narrowT0To()/genWideDivMod()/
        // genWideShift()).
        if (mnemonic == "srwi") {
            uint8_t rADest = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            uint8_t rSSrc = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
            uint8_t n = static_cast<uint8_t>(evalImm(requireOperand(ops, 2, mnemonic)) & 0x1F);
            uint8_t sh = static_cast<uint8_t>(32 - n);
            uint8_t mb = n;
            uint8_t me = 31;
            emit32((21u << 26) | (static_cast<uint32_t>(rSSrc) << 21) | (static_cast<uint32_t>(rADest) << 16) |
                   (static_cast<uint32_t>(sh) << 11) | (static_cast<uint32_t>(mb) << 6) |
                   (static_cast<uint32_t>(me) << 1));
            return;
        }
        // --- X-form arithmetic (direct field order) ---
        static const std::unordered_map<std::string, uint16_t> xArith = {
            {"add", 266}, {"mullw", 235}, {"divw", 491},
            // mulhwu (XO=11) gives the HIGH 32 bits of an unsigned
            // 32x32 product (mullw only ever gives the low 32) -
            // PowerPCCodegen.cpp's own genWideMul() needs it for the
            // cross-multiply carry term, the same role MIPS's
            // `mult`+`mfhi` or i386's `mul`'s own `edx` plays.
            // divwu (XO=459) is divw's unsigned counterpart.
            {"mulhwu", 11}, {"divwu", 459},
        };
        if (auto it = xArith.find(mnemonic); it != xArith.end()) {
            uint8_t rT = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            uint8_t rA = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
            uint8_t rB = requireReg(requireOperand(ops, 2, mnemonic), mnemonic);
            emitXArith(rT, rA, rB, it->second);
            return;
        }
        // subf rD,rA,rB = rB - rA (real PowerPC semantics - note the
        // operand meaning, not just the field order, is "backwards"
        // from a textbook subtract - see PowerPCCodegen.cpp's own
        // comment on how genBinary() accounts for it).
        if (mnemonic == "subf") {
            uint8_t rT = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            uint8_t rA = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
            uint8_t rB = requireReg(requireOperand(ops, 2, mnemonic), mnemonic);
            emitXArith(rT, rA, rB, 40);
            return;
        }
        // neg rD,rA = 0 - rA (rB field unused/zero).
        if (mnemonic == "neg") {
            uint8_t rT = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            uint8_t rA = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
            emitXArith(rT, rA, 0, 104);
            return;
        }
        // --- X-form logical (swapped field order) ---
        static const std::unordered_map<std::string, uint16_t> xLogical = {
            {"and", 28}, {"or", 444}, {"xor", 316}, {"nor", 124}, {"slw", 24}, {"srw", 536}, {"sraw", 792},
        };
        if (auto it = xLogical.find(mnemonic); it != xLogical.end()) {
            uint8_t rADest = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            uint8_t rSSrc = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
            uint8_t rB = requireReg(requireOperand(ops, 2, mnemonic), mnemonic);
            emitXLogical(rADest, rSSrc, rB, it->second);
            return;
        }
        // --- link register ---
        if (mnemonic == "mflr") {
            emitMoveLr(0x7C0802A6u, requireReg(requireOperand(ops, 0, mnemonic), mnemonic));
            return;
        }
        if (mnemonic == "mtlr") {
            emitMoveLr(0x7C0803A6u, requireReg(requireOperand(ops, 0, mnemonic), mnemonic));
            return;
        }
        if (mnemonic == "blr") {
            if (!ops.empty()) fail("'blr' takes no operand");
            emit32(0x4E800020u);
            return;
        }
        // --- count register (the indirect-call idiom - see
        // PowerPCCodegen.cpp's own CallPtr case) ---
        // mtctr rS - a real `mtspr 9,rS` (spr 9 is CTR): opcode 31,
        // rS<<21, spr split across bits 20-16/15-11 (9 fits entirely
        // in the low half, so the high half is 0), XO=467, exactly
        // the same field layout as `mtlr` above (spr 8) with the base
        // word's spr bits changed from 8 to 9 (0x7C0803A6 -> 0x7C0903A6).
        if (mnemonic == "mtctr") {
            emitMoveLr(0x7C0903A6u, requireReg(requireOperand(ops, 0, mnemonic), mnemonic));
            return;
        }
        // bctrl - Branch to Count Register, with Link: opcode 19,
        // BO=10100 (branch always), BI=0, XO=528, LK=1 - the real
        // documented PowerPC encoding 0x4E800421, the standard
        // second half of the `mtctr`/`bctrl` indirect-call idiom
        // (same relationship to `blr`'s own 0x4E800020 as `bctr`'s
        // XO field, 528, has to `bclr`'s, 16: 528<<1 = 0x420, so
        // bctrl's low halfword is 0x421 = 0x420 | the LK bit).
        if (mnemonic == "bctrl") {
            if (!ops.empty()) fail("'bctrl' takes no operand");
            emit32(0x4E800421u);
            return;
        }
        // --- dedicated zero-displacement load/store - see
        // PowerPCCodegen.h's own header comment: `ldind addrReg,
        // destReg` / `stind addrReg,srcReg`, real `lwz`/`stw`
        // underneath with a fixed displacement of 0. ---
        if (mnemonic == "ldind") {
            uint8_t rA = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            uint8_t rT = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
            emitDArith(32, rT, rA, 0);
            return;
        }
        if (mnemonic == "stind") {
            uint8_t rA = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            uint8_t rS = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
            emitDArith(36, rS, rA, 0);
            return;
        }
        // --- dedicated displacement load/store - see
        // PowerPCCodegen.h's own header comment on slotDisp(): real
        // `lwz`/`stw` underneath, exactly like `ldind`/`stind` above
        // except the displacement is this dialect's own third operand
        // (a plain signed immediate, never a register or label) rather
        // than always 0 - PowerPCCodegen.cpp's only use of real
        // PowerPC's own `d(rA)` displacement addressing, for a frame-
        // relative Local/Param access whose offset is a compile-time
        // constant. `ldindd addrReg,destReg,disp` / `stindd
        // addrReg,srcReg,disp`. ---
        if (mnemonic == "ldindd") {
            uint8_t rA = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            uint8_t rT = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
            uint16_t imm = evalImm(requireOperand(ops, 2, mnemonic));
            emitDArith(32, rT, rA, imm);
            return;
        }
        if (mnemonic == "stindd") {
            uint8_t rA = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            uint8_t rS = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
            uint16_t imm = evalImm(requireOperand(ops, 2, mnemonic));
            emitDArith(36, rS, rA, imm);
            return;
        }
        // Byte/halfword load (zero- or sign-extending into a full
        // register)/store (writing only that many low bytes), same
        // `addrReg,reg[,disp]` shape as ldindd/stindd above - real
        // LBZ(34)/LHZ(40)/LHA(42, sign-extending)/STB(38)/STH(44) - a
        // narrower-than-native-int Var/Slot's own load/store (see
        // PowerPCCodegen.cpp's loadOperand()/storeOperand()). PowerPC
        // has no sign-extending byte load at all (real ISA fact - no
        // "LBA"), so a signed `char`'s own sign-extension happens via
        // `srawi` after an ordinary LBZ instead (see narrowT0To()).
        static const std::unordered_map<std::string, uint8_t> sized = {
            {"ldindb", 34}, {"ldindh", 42}, {"ldindhu", 40}, {"stindb", 38}, {"stindh", 44},
        };
        if (auto it = sized.find(mnemonic); it != sized.end()) {
            uint8_t rA = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            uint8_t rT = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
            uint16_t imm = ops.size() > 2 ? evalImm(requireOperand(ops, 2, mnemonic)) : 0;
            emitDArith(it->second, rT, rA, imm);
            return;
        }
        // srawi rA,rS,SH - arithmetic shift right by an immediate
        // count (M-form special case, XO=824 - see narrowT0To()'s own
        // use for sign-extending a byte/halfword already in a
        // register).
        if (mnemonic == "srawi") {
            uint8_t rADest = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            uint8_t rSSrc = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
            uint8_t sh = static_cast<uint8_t>(evalImm(requireOperand(ops, 2, mnemonic)) & 0x1F);
            emit32((31u << 26) | (static_cast<uint32_t>(rSSrc) << 21) | (static_cast<uint32_t>(rADest) << 16) |
                   (static_cast<uint32_t>(sh) << 11) | (824u << 1));
            return;
        }
        // --- branches ---
        // A branch target defined in *this* object is already a valid
        // PC-relative displacement regardless of where this object
        // ends up linked (both its own address and this instruction's
        // own are shifted by the same base) - no relocation needed at
        // all, unlike an absolute-address load (`lis`/`ori` above).
        // Only a genuinely cross-object `bl` target (undefined here)
        // needs deferring - see PowerPCAssembler.h's own
        // `kPowerPCRelocBranch` comment, and
        // ../arm64/Arm64Assembler.cpp's own identical `b`/`bl`
        // handling this is ported from.
        if (mnemonic == "b" || mnemonic == "bl") {
            const Operand &target = requireOperand(ops, 0, mnemonic);
            if (target.kind != Operand::Mem) fail("'" + mnemonic + "': target must be a label");
            std::string externalName;
            uint32_t targetAddr = evalExpr(target.expr, &externalName);
            if (stage_ == 2 && allowExternal_ && !externalName.empty()) {
                relocations_.push_back({offset_, externalName, kPowerPCRelocBranch});
                emitBranch(0, mnemonic == "bl");
                return;
            }
            uint32_t rel = targetAddr - (orgBase_ + offset_);
            emitBranch(rel, mnemonic == "bl");
            return;
        }
        static const std::unordered_map<std::string, std::pair<uint8_t, uint8_t>> condBranches = {
            // {BO, BI}: BO=12 is "branch if true", BO=4 is "branch if
            // false"; BI selects crf0's LT(0)/GT(1)/EQ(2) bit - see
            // PowerPCCodegen.cpp's own genCompare() for why each
            // comparison operator picks the branch it does.
            {"blt", {12, 0}}, {"bge", {4, 0}}, {"bgt", {12, 1}}, {"ble", {4, 1}}, {"beq", {12, 2}}, {"bne", {4, 2}},
        };
        if (auto it = condBranches.find(mnemonic); it != condBranches.end()) {
            const Operand &target = requireOperand(ops, 0, mnemonic);
            if (target.kind != Operand::Mem) fail("'" + mnemonic + "': target must be a label");
            uint32_t targetAddr = evalExpr(target.expr);
            uint32_t rel = targetAddr - (orgBase_ + offset_);
            emitCondBranch(it->second.first, it->second.second, rel);
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
// Assembler produced - see PowerPCAssembler.h's own
// `assemblePowerPCObject` comment, and ../m68k/M68kAssembler.cpp's own
// `buildObjectFile` for the identical algorithm this is ported from.
ObjectFile buildObjectFile(const Assembler &a) {
    ObjectFile obj;
    obj.machineType = kPowerPCCoffMachine;
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

std::string assemblePowerPC(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/false);
    assembler.run(lines);
    const auto &bytes = assembler.bytes();
    return std::string(bytes.begin(), bytes.end());
}

ObjectFile assemblePowerPCObject(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/true);
    assembler.run(lines);
    return buildObjectFile(assembler);
}

}  // namespace pcc
