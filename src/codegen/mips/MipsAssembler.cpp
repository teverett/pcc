#include "MipsAssembler.h"

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

[[noreturn]] void fail(const std::string &msg) { throw std::runtime_error("MipsAssembler: " + msg); }

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
// MipsAssembler.h's own comment on why there's no real `0($t2)`/
// `%hi()`/`%lo()` MIPS syntax here. `Reg` comes from recognizing a
// bare register name; `Imm` from a bare (optionally signed) number;
// anything else (a label, or `label+N`) is `Mem` - an absolute
// address, resolved to the label's own full 32-bit value and then
// (for `lui`/`ori` specifically - see MipsAssembler.cpp's own
// encodeInstr()) split into its upper/lower half.
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

// This dialect's own fixed register numbering (not a real MIPS ABI
// register-name table in full - just the handful MipsCodegen.cpp
// ever emits - see MipsCodegen.h's own header comment on each
// register's role): `zero`=0, `at`=1, `t0`=8, `t1`=9, `t2`=10,
// `sp`=29, `ra`=31 - the real MIPS register numbers those names
// carry in every actual MIPS assembler too, so the encoded bit
// patterns below match genuine MIPS object code exactly, even
// though this dialect only ever names this one subset of the 32
// GPRs. Returns -1 if `name` isn't one of them.
int regNumber(const std::string &lower) {
    static const std::unordered_map<std::string, int> names = {
        {"zero", 0}, {"at", 1}, {"t0", 8}, {"t1", 9}, {"t2", 10}, {"sp", 29}, {"ra", 31},
    };
    auto it = names.find(lower);
    return it == names.end() ? -1 : it->second;
}

Operand buildOperand(PccAsmParser::OperandContext *ctx) {
    if (ctx->LBRACKET()) {
        fail(
            "bracketed memory operands aren't part of this target's dialect - MIPS memory access always goes "
            "through the dedicated 'ldind'/'stind' mnemonics instead (see MipsCodegen.h)");
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

// One pending fixup `assembleMipsObject()` (see MipsAssembler.h's own
// header comment) turns into a real ObjRelocation once assembly is
// done - `offset` into `bytes()`, `symbolName` the reference that
// couldn't be resolved locally (or the reserved pseudo-symbol ".."
// for a same-file address's own needed base-shift), `kind` one of
// kMipsRelocHi/Lo/Jump.
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
            case Line::Instr:
                encodeInstr(line.mnemonic, line.operands);
                return;
        }
    }

    // `externalName`/`needsBaseShift` mirror
    // ../m68k/M68kAssembler.cpp's own `evalExpr` exactly - see
    // MipsAssembler.h's own header comment on why *any* local address
    // reference, not just an external one, needs a relocation here
    // once `allowExternal_` is set.
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

    static uint16_t imm16Of(int32_t v) {
        if (v < -32768 || v > 65535) fail("immediate out of 16-bit range");
        return static_cast<uint16_t>(v & 0xFFFF);
    }

    // The signed/unsigned decimal `op`'s own exprList already carries
    // (e.g. `addiu t0,zero,-4`), masked to 16 bits.
    uint16_t evalImm(const Operand &op) {
        if (op.kind == Operand::Reg) fail("expected an immediate or label operand, not a register");
        int32_t v = static_cast<int32_t>(evalExpr(op.expr));
        return imm16Of(v);
    }

    // `lui`/`ori`'s own immediate operand: a plain number is used
    // exactly as MipsCodegen.cpp already computed it (that half of a
    // literal it split itself - see IntLit's own codegen); a label
    // (`Mem`) instead has its own full resolved address split into
    // its upper (`lui`) or lower (`ori`) 16 bits automatically here -
    // see MipsAssembler.h's own header comment on why this dialect
    // needs no `%hi`/`%lo` syntax at all. `externalName`/
    // `needsBaseShift` are only ever meaningful for a `Mem` operand -
    // see `encodeHiLo()`'s own call site.
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

    // --- MIPS instruction formats ---------------------------------------
    // R-type: opcode(6) rs(5) rt(5) rd(5) shamt(5) funct(6).
    void emitR(uint8_t opcode, uint8_t rs, uint8_t rt, uint8_t rd, uint8_t shamt, uint8_t funct) {
        emit32((static_cast<uint32_t>(opcode) << 26) | (static_cast<uint32_t>(rs) << 21) |
               (static_cast<uint32_t>(rt) << 16) | (static_cast<uint32_t>(rd) << 11) |
               (static_cast<uint32_t>(shamt) << 6) | funct);
    }
    // I-type: opcode(6) rs(5) rt(5) imm16.
    void emitI(uint8_t opcode, uint8_t rs, uint8_t rt, uint16_t imm) {
        emit32((static_cast<uint32_t>(opcode) << 26) | (static_cast<uint32_t>(rs) << 21) |
               (static_cast<uint32_t>(rt) << 16) | imm);
    }
    // J-type: opcode(6) instr_index(26) - a real MIPS jump's own
    // target is this program's own flat 32-bit address shifted right
    // 2 (word-aligned) and truncated to 26 bits, exactly like any
    // real MIPS assembler/linker computes it.
    void emitJ(uint8_t opcode, uint32_t targetAddr) {
        emit32((static_cast<uint32_t>(opcode) << 26) | ((targetAddr >> 2) & 0x03FFFFFFu));
    }

    void encodeRRR(uint8_t funct, const std::vector<Operand> &ops, const std::string &mnemonic) {
        // dialect order: mnemonic rd,rs,rt
        uint8_t rd = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        uint8_t rs = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
        uint8_t rt = requireReg(requireOperand(ops, 2, mnemonic), mnemonic);
        emitR(0, rs, rt, rd, 0, funct);
    }
    // SLLV/SRLV - dialect order: mnemonic rd,rt,rs (rs is the shift-
    // count register - real MIPS field order, see MipsCodegen.h's own
    // header comment on why the dialect spells the count operand
    // last, matching how it's actually read in hardware).
    void encodeShiftV(uint8_t funct, const std::vector<Operand> &ops, const std::string &mnemonic) {
        uint8_t rd = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        uint8_t rt = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
        uint8_t rs = requireReg(requireOperand(ops, 2, mnemonic), mnemonic);
        emitR(0, rs, rt, rd, 0, funct);
    }
    // SLL - dialect order: mnemonic rd,rt,shamt (shamt is a literal
    // 0-31 constant, not a register).
    void encodeShiftImm(uint8_t funct, const std::vector<Operand> &ops, const std::string &mnemonic) {
        uint8_t rd = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        uint8_t rt = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
        uint8_t shamt = static_cast<uint8_t>(evalImm(requireOperand(ops, 2, mnemonic)) & 0x1F);
        emitR(0, 0, rt, rd, shamt, funct);
    }
    // ADDIU/SLTIU - dialect order: mnemonic rt,rs,imm. Never takes a
    // label operand (see MipsCodegen.cpp's own call sites - always a
    // plain literal), so unlike `encodeHiLo()` below this never needs
    // to track a relocation.
    void encodeImmArith(uint8_t opcode, const std::vector<Operand> &ops, const std::string &mnemonic) {
        uint8_t rt = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        uint8_t rs = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
        uint16_t imm = evalImm(requireOperand(ops, 2, mnemonic));
        emitI(opcode, rs, rt, imm);
    }

    // LUI's own `rt,imm` / ORI's own `rt,rs,imm` immediate operand,
    // when it's a label (`Mem`): every such reference - even to a
    // label defined in this same file - gets a relocation against the
    // reserved pseudo-symbol ".." (this object assembles with `org 0`
    // - see MipsAssembler.h's own header comment), and an undefined
    // one additionally (or instead) gets one against its own name.
    // `relocKind` is `kMipsRelocHi` for `lui`, `kMipsRelocLo` for
    // `ori` - see MipsLinkerFactory.cpp's own paired Hi/Lo patch
    // functions for why `lui` must always immediately precede `ori`
    // for the same address (both the 4-byte-apart site offsets *and*
    // the relocation-list order that gives the Hi patch access to the
    // Lo site's own still-unpatched value both come from nothing more
    // than MipsCodegen.cpp's own `loadAddress()` always emitting them
    // back to back, and this function recording each one's relocation
    // the moment it's encoded).
    void encodeHiLo(uint8_t opcode, uint8_t rs, uint8_t rt, const Operand &immOp, bool hi, uint16_t relocKind) {
        std::string externalName;
        bool needsBaseShift = false;
        uint16_t imm = evalHiLo(immOp, hi, &externalName, &needsBaseShift);
        if (stage_ == 2 && allowExternal_ && immOp.kind == Operand::Mem) {
            if (needsBaseShift) relocations_.push_back({offset_, "..", relocKind});
            if (!externalName.empty()) relocations_.push_back({offset_, externalName, relocKind});
        }
        emitI(opcode, rs, rt, imm);
    }

    // The full instruction vocabulary ../MipsCodegen.cpp ever emits -
    // see MipsAssembler.h's own header comment. Not a general MIPS
    // encoder: exactly these mnemonic/operand-shape combinations.
    void encodeInstr(const std::string &mnemonic, const std::vector<Operand> &ops) {
        if (mnemonic == "nop") {
            if (!ops.empty()) fail("'nop' takes no operand");
            emit32(0);
            return;
        }
        static const std::unordered_map<std::string, uint8_t> rrr = {
            {"addu", 0x21}, {"subu", 0x23}, {"and", 0x24}, {"or", 0x25}, {"xor", 0x26}, {"nor", 0x27},
            {"slt", 0x2A},  {"sltu", 0x2B},
        };
        if (auto it = rrr.find(mnemonic); it != rrr.end()) {
            encodeRRR(it->second, ops, mnemonic);
            return;
        }
        if (mnemonic == "sllv" || mnemonic == "srlv") {
            encodeShiftV(mnemonic == "sllv" ? 0x04 : 0x06, ops, mnemonic);
            return;
        }
        if (mnemonic == "sll") {
            encodeShiftImm(0x00, ops, mnemonic);
            return;
        }
        if (mnemonic == "addiu") {
            encodeImmArith(0x09, ops, mnemonic);
            return;
        }
        if (mnemonic == "sltiu") {
            encodeImmArith(0x0B, ops, mnemonic);
            return;
        }
        if (mnemonic == "ori") {
            uint8_t rt = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            uint8_t rs = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
            encodeHiLo(0x0D, rs, rt, requireOperand(ops, 2, mnemonic), /*hi=*/false, kMipsRelocLo);
            return;
        }
        if (mnemonic == "lui") {
            uint8_t rt = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            encodeHiLo(0x0F, 0, rt, requireOperand(ops, 1, mnemonic), /*hi=*/true, kMipsRelocHi);
            return;
        }
        // Dedicated zero-displacement load/store - see MipsCodegen.h's
        // own header comment: `ldind addrReg,destReg` / `stind
        // addrReg,srcReg`, real LW/SW underneath with imm16=0.
        if (mnemonic == "ldind") {
            uint8_t base = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            uint8_t rt = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
            emitI(0x23, base, rt, 0);
            return;
        }
        if (mnemonic == "stind") {
            uint8_t base = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            uint8_t rt = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
            emitI(0x2B, base, rt, 0);
            return;
        }
        if (mnemonic == "mult" || mnemonic == "div") {
            uint8_t rs = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            uint8_t rt = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
            emitR(0, rs, rt, 0, 0, mnemonic == "mult" ? 0x18 : 0x1A);
            return;
        }
        if (mnemonic == "mflo" || mnemonic == "mfhi") {
            uint8_t rd = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            emitR(0, 0, 0, rd, 0, mnemonic == "mflo" ? 0x12 : 0x10);
            return;
        }
        if (mnemonic == "jr") {
            uint8_t rs = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            emitR(0, rs, 0, 0, 0, 0x08);
            return;
        }
        if (mnemonic == "j" || mnemonic == "jal") {
            const Operand &target = requireOperand(ops, 0, mnemonic);
            if (target.kind != Operand::Mem) fail("'" + mnemonic + "': target must be a label");
            // Unlike BEQ/BNE below (real PC-relative branches, never
            // used to reach another file's own code - see
            // MipsCodegen.h's own header comment), J/JAL's own target
            // is this dialect's one pseudo-absolute address field, so
            // even a purely intra-function jump (an `if`/`while`'s own
            // label, never actually cross-object) needs the same ".."
            // base-shift relocation a genuine cross-object call does -
            // see MipsAssembler.h's own header comment on why *every*
            // local address reference needs one here.
            std::string externalName;
            bool needsBaseShift = false;
            uint32_t targetAddr = evalExpr(target.expr, &externalName, &needsBaseShift);
            if (stage_ == 2 && allowExternal_) {
                if (needsBaseShift) relocations_.push_back({offset_, "..", kMipsRelocJump});
                if (!externalName.empty()) relocations_.push_back({offset_, externalName, kMipsRelocJump});
            }
            emitJ(mnemonic == "j" ? 0x02 : 0x03, targetAddr);
            return;
        }
        if (mnemonic == "beq" || mnemonic == "bne") {
            uint8_t rs = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            uint8_t rt = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
            const Operand &target = requireOperand(ops, 2, mnemonic);
            if (target.kind != Operand::Mem) fail("'" + mnemonic + "': target must be a label");
            uint32_t targetAddr = evalExpr(target.expr);
            // Branch displacement is relative to the delay-slot
            // instruction's own address (the branch's own address +
            // 4), measured in words - real MIPS PC-relative branch
            // semantics.
            uint32_t branchEnd = orgBase_ + offset_ + 4;
            int32_t rel = (static_cast<int32_t>(targetAddr) - static_cast<int32_t>(branchEnd)) / 4;
            if (stage_ == 2 && (rel < -32768 || rel > 32767)) fail("branch target out of 16-bit range");
            emitI(mnemonic == "beq" ? 0x04 : 0x05, rs, rt, static_cast<uint16_t>(static_cast<int16_t>(rel)));
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
// Assembler produced - see MipsAssembler.h's own `assembleMipsObject`
// comment, and ../m68k/M68kAssembler.cpp's own `buildObjectFile` for
// the identical algorithm this is ported from.
ObjectFile buildObjectFile(const Assembler &a) {
    ObjectFile obj;
    obj.machineType = kMipsCoffMachine;
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

std::string assembleMips(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/false);
    assembler.run(lines);
    const auto &bytes = assembler.bytes();
    return std::string(bytes.begin(), bytes.end());
}

ObjectFile assembleMipsObject(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/true);
    assembler.run(lines);
    return buildObjectFile(assembler);
}

}  // namespace pcc
