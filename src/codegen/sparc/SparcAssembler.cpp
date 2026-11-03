#include "SparcAssembler.h"

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

[[noreturn]] void fail(const std::string &msg) { throw std::runtime_error("SparcAssembler: " + msg); }

// --- Parsing, via the one shared grammar/PccAsm.g4 -----------------------
// Same Syllable/Group shape as every other backend's own assembler -
// see PowerPCAssembler.cpp's own comment.
enum class SylKind { Number, Symbol, Group };
struct Syllable {
    char sign = 0;
    SylKind kind;
    std::string text;
    std::vector<Syllable> group;
};

// One operand, classified from a bare expression - see
// SparcAssembler.h's own comment on why there's no real
// `[%rs1+off]`/`%hi()`/`%lo()` SPARC syntax here. `Reg` comes from
// recognizing a bare register name; `Imm` from a bare (optionally
// signed) number; anything else (a label, or `label+N`) is `Mem` - an
// absolute address, resolved to the label's own full 32-bit value and
// then (for `sethi`/`or` specifically - see encodeInstr()) split into
// its upper 22 / lower 10 bits.
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

// This dialect's own fixed register numbering (not a real SPARC ABI
// register-name table in full - just the handful SparcCodegen.cpp
// ever emits - see SparcCodegen.h's own header comment for each
// register's role): `g0`=0 (the real hardwired-zero register), `t0`=8
// (%o0 - also the real SPARC ABI's own return-value register, though
// that convention is otherwise unused here), `t1`=9 (%o1), `t2`=10
// (%o2), `sp`=14 (%o6 - the real SPARC stack-pointer register number,
// even though this dialect never executes a real `save`/`restore`
// window shift), `lr`=15 (%o7 - the real SPARC register `call`
// itself writes the return address into). The encoded bit patterns
// below match genuine SPARC register numbers exactly, even though
// this dialect only ever names this one subset of the 32 integer
// registers. Returns -1 if `name` isn't one of them.
int regNumber(const std::string &lower) {
    static const std::unordered_map<std::string, int> names = {
        {"g0", 0}, {"t0", 8}, {"t1", 9}, {"t2", 10}, {"sp", 14}, {"lr", 15},
    };
    auto it = names.find(lower);
    return it == names.end() ? -1 : it->second;
}

Operand buildOperand(PccAsmParser::OperandContext *ctx) {
    if (ctx->LBRACKET()) {
        fail(
            "bracketed memory operands aren't part of this target's dialect - SPARC memory access always goes "
            "through the dedicated 'ldind'/'stind' mnemonics instead (see SparcCodegen.h)");
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

// One pending fixup `assembleSparcObject()` (see SparcAssembler.h's
// own header comment) turns into a real ObjRelocation once assembly
// is done - `offset` into `bytes()`, `symbolName` the reference that
// couldn't be resolved locally (or the reserved pseudo-symbol ".."
// for a same-file address's own needed base-shift), `kind` one of
// kSparcRelocHi/Lo/Call.
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
                // external-name/base-shift tracking as `sethi`/`or`'s own
                // Hi/Lo immediate (see encodeInstr()) - a bare symbol here
                // (an array initializer's address-of-function/global
                // element) resolves and relocates as a plain additive
                // 32-bit word (kSparcRelocAbs32), rather than the split
                // Hi/Lo pair an instruction's own immediate field needs.
                for (const auto &e : line.exprList) {
                    std::string externalName;
                    bool needsBaseShift = false;
                    uint32_t v = evalExpr(e, &externalName, &needsBaseShift);
                    if (stage_ == 2 && allowExternal_) {
                        if (needsBaseShift) relocations_.push_back({offset_, "..", kSparcRelocAbs32});
                        if (!externalName.empty()) relocations_.push_back({offset_, externalName, kSparcRelocAbs32});
                    }
                    emit32(v);
                }
                return;
            case Line::Instr:
                encodeInstr(line.mnemonic, line.operands);
                return;
        }
    }

    // `externalName`/`needsBaseShift` mirror
    // ../powerpc/PowerPCAssembler.cpp's own `evalExpr` exactly - see
    // SparcAssembler.h's own header comment on why *any* local
    // `sethi`/`or` address reference, not just an external one, needs
    // a relocation here once `allowExternal_` is set (a `call`/`ba`/
    // `b<cc>` branch target never sets `needsBaseShift` at all - its
    // own caller never passes that out-param, since a same-object
    // control-transfer displacement needs no relocation regardless of
    // where this object ends up linked).
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
    void emit32(uint32_t v) {
        bytes_.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
        bytes_.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
        bytes_.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        bytes_.push_back(static_cast<uint8_t>(v & 0xFF));
        offset_ += 4;
    }
    // Real SPARC `nop` - `sethi 0,%g0` (format 2, op=00 rd=00000
    // op2=100 imm22=0, i.e. word 0x01000000 - *not* all-zero: a plain
    // zero word is real SPARC's own `unimp` trap instruction, since
    // op2=000 rather than sethi's own 100). Always synthesized into a
    // control-transfer instruction's own delay slot - see
    // SparcAssembler.h's own header comment.
    void emitNop() { emit32(0b100u << 22); }

    static uint16_t imm13Of(int32_t v) {
        if (v < -4096 || v > 4095) fail("immediate out of signed 13-bit range");
        return static_cast<uint16_t>(v & 0x1FFF);
    }

    // The plain decimal `op`'s own exprList already carries, masked
    // to a signed 13-bit field - every format-3 instruction's own
    // immediate operand in this dialect (SparcCodegen.cpp only ever
    // passes a small literal constant this way, never a label).
    uint16_t evalImm(const Operand &op) {
        if (op.kind != Operand::Imm) fail("expected a plain immediate operand, not a register or label");
        int32_t v = static_cast<int32_t>(evalExpr(op.expr));
        return imm13Of(v);
    }

    static uint8_t requireReg(const Operand &op, const std::string &mnemonic) {
        if (op.kind != Operand::Reg) fail("'" + mnemonic + "' needs a register operand");
        return static_cast<uint8_t>(op.reg);
    }
    static const Operand &requireOperand(const std::vector<Operand> &ops, size_t i, const std::string &mnemonic) {
        if (ops.size() <= i) fail("'" + mnemonic + "': missing operand");
        return ops[i];
    }

    // --- SPARC V8 instruction formats -----------------------------------
    // Format 3 (op=2 arithmetic/logical, op=3 load/store): op(2)
    // rd(5) op3(6) rs1(5) i(1) [asi(8) rs2(5) | simm13(13)] - the one
    // format every ALU/compare/shift/multiply/divide/load/store
    // mnemonic in this dialect goes through. `operand2` is a Reg
    // (register form, i=0) or Imm (immediate form, i=1, the field
    // sign-extended from 13 bits) - SparcCodegen.cpp never passes a
    // Mem (label) operand here; only `sethi`/`or`'s own dedicated
    // handling (see encodeInstr()) ever resolves a label.
    uint32_t fmt3(uint8_t op, uint8_t rd, uint8_t op3, uint8_t rs1, const Operand &operand2) {
        uint32_t word = (static_cast<uint32_t>(op) << 30) | (static_cast<uint32_t>(rd) << 25) |
                         (static_cast<uint32_t>(op3) << 19) | (static_cast<uint32_t>(rs1) << 14);
        if (operand2.kind == Operand::Reg) {
            word |= static_cast<uint32_t>(operand2.reg) & 0x1Fu;
        } else {
            word |= (1u << 13) | evalImm(operand2);
        }
        return word;
    }
    // The register-rs1/register-or-immediate-rs2/register-rd family
    // (add/sub/and/or/xor/sll/srl/sra/smul/sdiv) - `op`'s own operand
    // order matches genuine SPARC assembly syntax exactly (source(s)
    // first, destination last), unlike every other backend in this
    // project (which all put the destination first) - see
    // SparcCodegen.h's own header comment.
    void encodeAlu3(uint8_t op3, const std::vector<Operand> &ops, const std::string &mnemonic) {
        uint8_t rs1 = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        const Operand &op2 = requireOperand(ops, 1, mnemonic);
        uint8_t rd = requireReg(requireOperand(ops, 2, mnemonic), mnemonic);
        emit32(fmt3(2, rd, op3, rs1, op2));
    }

    // `sethi`'s own 22-bit immediate field (format 2: op=0 rd(5)
    // op2=100(3) imm22(22)) - a plain `Imm` operand is used exactly as
    // SparcCodegen.cpp already computed it (that half of a literal it
    // split itself - see its own `genExpr()`'s IntLit case); a label
    // (`Mem`) operand instead has its own full resolved address split
    // into its upper 22 bits automatically here - see
    // SparcAssembler.h's own header comment on why this dialect needs
    // no `%hi`/`%lo` syntax spelled out at all.
    void encodeSethi(const std::vector<Operand> &ops) {
        uint8_t rd = requireReg(requireOperand(ops, 1, "sethi"), "sethi");
        const Operand &immOp = requireOperand(ops, 0, "sethi");
        std::string externalName;
        bool needsBaseShift = false;
        uint32_t full = evalExpr(immOp.expr, &externalName, &needsBaseShift);
        uint32_t imm22 = immOp.kind == Operand::Mem ? (full >> 10) & 0x3FFFFFu : full & 0x3FFFFFu;
        if (stage_ == 2 && allowExternal_ && immOp.kind == Operand::Mem) {
            if (needsBaseShift) relocations_.push_back({offset_, "..", kSparcRelocHi});
            if (!externalName.empty()) relocations_.push_back({offset_, externalName, kSparcRelocHi});
        }
        emit32((0u << 30) | (static_cast<uint32_t>(rd) << 25) | (0b100u << 22) | imm22);
    }

    // `or`'s own three real uses in this dialect, all the same real
    // SPARC OR instruction (format 3, op3=0x02): a plain register
    // second operand (the language's own `|` operator, and the
    // `or g0,src,dst` "move register" idiom - see
    // SparcCodegen.cpp's own `movReg()`), a plain immediate second
    // operand (`or g0,<imm>,dst` - loading a small literal, or the
    // lower-10-bits half of a literal too wide for `sethi` alone to
    // carry), or a label (`Mem`) second operand - the lower-10-bits
    // half of the same `sethi`/`or` address-load pair above; same
    // relocation treatment as `sethi`, `kSparcRelocLo` instead of
    // `kSparcRelocHi` - see SparcLinkerFactory.cpp's own paired Hi/Lo
    // patch functions for how the two sites' own carry gets computed
    // correctly. Real SPARC assembly syntax for this exact
    // three-way reuse is identical (`or %rs1, %lo(label), %rd` is
    // genuinely plain `or`, not a distinct mnemonic, in real SPARC
    // code) - so unlike PowerPC's own `lis`/`ori` split (two distinct
    // real opcodes), this dialect needs only one mnemonic for both
    // halves of the pair.
    void encodeOr(const std::vector<Operand> &ops) {
        uint8_t rs1 = requireReg(requireOperand(ops, 0, "or"), "or");
        const Operand &op2 = requireOperand(ops, 1, "or");
        uint8_t rd = requireReg(requireOperand(ops, 2, "or"), "or");
        if (op2.kind != Operand::Mem) {
            emit32(fmt3(2, rd, 0x02, rs1, op2));
            return;
        }
        std::string externalName;
        bool needsBaseShift = false;
        uint32_t full = evalExpr(op2.expr, &externalName, &needsBaseShift);
        uint32_t lo10 = full & 0x3FFu;
        if (stage_ == 2 && allowExternal_) {
            if (needsBaseShift) relocations_.push_back({offset_, "..", kSparcRelocLo});
            if (!externalName.empty()) relocations_.push_back({offset_, externalName, kSparcRelocLo});
        }
        emit32((2u << 30) | (static_cast<uint32_t>(rd) << 25) | (0x02u << 19) | (static_cast<uint32_t>(rs1) << 14) |
               (1u << 13) | lo10);
    }

    // `cmp rs1,rs2_or_imm` - real `subcc rs1,rs2_or_imm,%g0`: sets the
    // integer condition codes to `rs1 - rs2_or_imm` without keeping
    // the (discarded) difference itself, the one way SPARC has to
    // compare two values (no standalone compare instruction - see
    // SparcCodegen.h's own header comment).
    void encodeCmp(const std::vector<Operand> &ops) {
        uint8_t rs1 = requireReg(requireOperand(ops, 0, "cmp"), "cmp");
        const Operand &op2 = requireOperand(ops, 1, "cmp");
        emit32(fmt3(2, 0, 0x14, rs1, op2));
    }

    // `wry rs1,rs2_or_imm` - real `wr rs1,rs2_or_imm,%y`: sets the Y
    // register (ASR 0) to `rs1 XOR rs2_or_imm` - used only to seed Y
    // with the dividend's own sign bits (via a preceding `sra ...,31,`
    // - see SparcCodegen.cpp's own `genDivMod()`) before `sdiv`, which
    // (like every SPARC V8 32-bit divide) computes a 64-bit `Y:rs1`
    // dividend under the hood.
    void encodeWry(const std::vector<Operand> &ops) {
        uint8_t rs1 = requireReg(requireOperand(ops, 0, "wry"), "wry");
        const Operand &op2 = requireOperand(ops, 1, "wry");
        emit32(fmt3(2, 0, 0x30, rs1, op2));
    }

    // `ldind`/`stind` - see SparcCodegen.h's own header comment:
    // dedicated, always-zero-displacement mnemonics standing in for
    // real `ld [rs1+0],rd` / `st rd,[rs1+0]` (format 3, op=3), since
    // this dialect's own grammar has no bracketed memory-operand
    // syntax (see SparcAssembler.h's own header comment) and every
    // load/store this backend ever emits is through a freshly
    // computed address with no other displacement needed anyway.
    void encodeLdind(const std::vector<Operand> &ops) {
        uint8_t rs1 = requireReg(requireOperand(ops, 0, "ldind"), "ldind");
        uint8_t rd = requireReg(requireOperand(ops, 1, "ldind"), "ldind");
        emit32((3u << 30) | (static_cast<uint32_t>(rd) << 25) | (0x00u << 19) | (static_cast<uint32_t>(rs1) << 14) |
               (1u << 13));
    }
    void encodeStind(const std::vector<Operand> &ops) {
        uint8_t rs1 = requireReg(requireOperand(ops, 0, "stind"), "stind");
        uint8_t rd = requireReg(requireOperand(ops, 1, "stind"), "stind");
        emit32((3u << 30) | (static_cast<uint32_t>(rd) << 25) | (0x04u << 19) | (static_cast<uint32_t>(rs1) << 14) |
               (1u << 13));
    }

    // `call label` - real SPARC CALL (format 1: op=01 disp30(30)) -
    // always immediately followed by a synthesized `nop` delay-slot
    // instruction (see SparcAssembler.h's own header comment). A
    // target defined in *this* object is already a valid PC-relative
    // displacement regardless of where this object ends up linked
    // (both its own address and this instruction's own are shifted by
    // the same base) - no relocation needed at all, unlike an
    // absolute-address load (`sethi`/`or` above). Only a genuinely
    // cross-object `call` target (undefined here) needs deferring -
    // see SparcAssembler.h's own `kSparcRelocCall` comment, and
    // PowerPCAssembler.cpp's own identical `b`/`bl` handling this is
    // ported from.
    void encodeCall(const std::vector<Operand> &ops) {
        const Operand &target = requireOperand(ops, 0, "call");
        if (target.kind != Operand::Mem) fail("'call': target must be a label");
        std::string externalName;
        uint32_t targetAddr = evalExpr(target.expr, &externalName);
        if (stage_ == 2 && allowExternal_ && !externalName.empty()) {
            relocations_.push_back({offset_, externalName, kSparcRelocCall});
            emit32(1u << 30);
            emitNop();
            return;
        }
        uint32_t rel = targetAddr - (orgBase_ + offset_);
        emit32((1u << 30) | ((rel >> 2) & 0x3FFFFFFFu));
        emitNop();
    }

    // `retl` - real SPARC `jmpl %lr+8,%g0` (format 3, op=2, op3=0x38
    // JMPL): the leaf-routine return idiom this dialect's own flat
    // calling convention always uses (no real register-window
    // `save`/`restore` - see SparcCodegen.h's own header comment),
    // immediately followed by a synthesized `nop` delay slot.
    void encodeRetl() {
        emit32((2u << 30) | (0u << 25) | (0x38u << 19) | (15u << 14) | (1u << 13) | 8u);
        emitNop();
    }

    // `jmpl rs1,rd` - real SPARC JMPL (format 3, op=2, op3=0x38),
    // register-indirect form (i=0, rs2=%g0, so the effective target
    // is simply rs1's own value - the same fixed-zero-displacement
    // convention ldind/stind already use, see SparcCodegen.h's own
    // header comment): PC <- rs1, rd <- the address of this
    // instruction itself, the identical return-address convention
    // `call` already leaves in its own target register for a direct
    // call - see encodeCall()'s own comment - so SparcCodegen.cpp's
    // own CallPtr case can target `lr` here and let the callee's
    // already-existing `retl` ("jmpl lr+8,g0") epilogue return
    // correctly either way. Always immediately followed by a
    // synthesized `nop` delay slot, same as every other real
    // control-transfer here.
    void encodeJmpl(const std::vector<Operand> &ops) {
        uint8_t rs1 = requireReg(requireOperand(ops, 0, "jmpl"), "jmpl");
        uint8_t rd = requireReg(requireOperand(ops, 1, "jmpl"), "jmpl");
        emit32((2u << 30) | (static_cast<uint32_t>(rd) << 25) | (0x38u << 19) | (static_cast<uint32_t>(rs1) << 14));
        emitNop();
    }

    // Bicc - format 2: op=0 a(1)=0 cond(4) op2=010(3) disp22(22).
    // `cond`'s own real SPARC encoding - see SparcCodegen.cpp's own
    // `genCompare()` for which comparison operator picks which one.
    // Always immediately followed by a synthesized `nop` delay slot.
    // A branch target here is always intra-function-only (this
    // dialect's own `beq`/`bne`/etc. are never used to reach another
    // file's own code - same convention, and the same reason, as
    // M68kCodegen.h's own `Bcc`/`BRA`), so (like a real-architecture
    // PC-relative branch defined in the same object - see
    // PowerPCAssembler.h's own `kPowerPCRelocBranch` comment) it never
    // needs a relocation at all.
    void encodeBicc(uint8_t cond, const std::vector<Operand> &ops, const std::string &mnemonic) {
        const Operand &target = requireOperand(ops, 0, mnemonic);
        if (target.kind != Operand::Mem) fail("'" + mnemonic + "': target must be a label");
        uint32_t targetAddr = evalExpr(target.expr);
        int32_t rel = static_cast<int32_t>(targetAddr) - static_cast<int32_t>(orgBase_ + offset_);
        if (stage_ == 2 && (rel < -(1 << 23) || rel >= (1 << 23))) fail("branch target out of 22-bit range");
        emit32((0u << 30) | (static_cast<uint32_t>(cond) << 25) | (0b010u << 22) |
               ((static_cast<uint32_t>(rel) >> 2) & 0x3FFFFFu));
        emitNop();
    }

    void encodeInstr(const std::string &mnemonic, const std::vector<Operand> &ops) {
        static const std::unordered_map<std::string, uint8_t> alu3 = {
            {"add", 0x00}, {"sub", 0x04}, {"and", 0x01}, {"xor", 0x03},
            {"sll", 0x25}, {"srl", 0x26}, {"sra", 0x27}, {"smul", 0x0B}, {"sdiv", 0x0F},
        };
        if (auto it = alu3.find(mnemonic); it != alu3.end()) {
            encodeAlu3(it->second, ops, mnemonic);
            return;
        }
        if (mnemonic == "or") {
            encodeOr(ops);
            return;
        }
        if (mnemonic == "sethi") {
            encodeSethi(ops);
            return;
        }
        if (mnemonic == "cmp") {
            encodeCmp(ops);
            return;
        }
        if (mnemonic == "wry") {
            encodeWry(ops);
            return;
        }
        if (mnemonic == "ldind") {
            encodeLdind(ops);
            return;
        }
        if (mnemonic == "stind") {
            encodeStind(ops);
            return;
        }
        if (mnemonic == "call") {
            encodeCall(ops);
            return;
        }
        if (mnemonic == "jmpl") {
            encodeJmpl(ops);
            return;
        }
        if (mnemonic == "retl") {
            if (!ops.empty()) fail("'retl' takes no operand");
            encodeRetl();
            return;
        }
        if (mnemonic == "nop") {
            if (!ops.empty()) fail("'nop' takes no operand");
            emitNop();
            return;
        }
        if (mnemonic == "ba") {
            encodeBicc(0x8, ops, mnemonic);
            return;
        }
        static const std::unordered_map<std::string, uint8_t> condBranches = {
            {"beq", 0x1}, {"bne", 0x9}, {"blt", 0x3}, {"ble", 0x2}, {"bgt", 0xA}, {"bge", 0xB},
        };
        if (auto it = condBranches.find(mnemonic); it != condBranches.end()) {
            encodeBicc(it->second, ops, mnemonic);
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
// Assembler produced - see SparcAssembler.h's own
// `assembleSparcObject` comment, and ../powerpc/PowerPCAssembler.cpp's
// own `buildObjectFile` for the identical algorithm this is ported
// from.
ObjectFile buildObjectFile(const Assembler &a) {
    ObjectFile obj;
    obj.machineType = kSparcCoffMachine;
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

std::string assembleSparc(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/false);
    assembler.run(lines);
    const auto &bytes = assembler.bytes();
    return std::string(bytes.begin(), bytes.end());
}

ObjectFile assembleSparcObject(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/true);
    assembler.run(lines);
    return buildObjectFile(assembler);
}

}  // namespace pcc
