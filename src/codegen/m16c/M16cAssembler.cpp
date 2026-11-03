#include "M16cAssembler.h"

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

[[noreturn]] void fail(const std::string &msg) { throw std::runtime_error("M16cAssembler: " + msg); }

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
// M16cAssembler.h's own header comment on why there's no real
// `[A0]`/`#n` MACRO-style syntax here. `Reg` comes from recognizing a
// bare `r0`-`r3`/`a0`/`a1`/`sp`/`fb` identifier; `Imm` from a bare
// (optionally signed) number; anything else without a bracket (a
// label, or `label+N`) is `Mem` - an absolute address. `Disp` is the
// one bracketed shape this dialect does support - `[fb+N]`/`[fb-N]`,
// a frame-base-register-relative address (see M16cCodegen.h's
// `frameOperand()`): `reg` holds the base register, `expr` the
// (possibly negative) word-offset displacement.
struct Operand {
    enum Kind { Reg, Imm, Mem, Disp } kind = Imm;
    int reg = -1;
    std::vector<Syllable> expr;  // Imm/Mem/Disp
};

struct Line {
    std::vector<std::string> labels;
    enum Kind { None, Org, Dw, Instr } kind = None;
    std::vector<std::vector<Syllable>> exprList;  // Org (1)/Dw (N)
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

// `r0`-`r3` (case-insensitive, matching how mnemonics are lowercased
// too), plus `a0`/`a1` (address registers), `sp` (the real hardware
// stack pointer) and `fb` (the real hardware frame-base register,
// used for every Local/Param's own per-call frame address - see
// M16cCodegen.h's `frameOperand()`) - returns -1 if `name` is none of
// those. Register codes 0-3 are R0-R3, 4-5 are A0-A1, 6 is SP, 7 is
// FB - an arbitrary but fixed numbering, this dialect's own (see
// M16cAssembler.h's own header comment: not Renesas's real
// register-field encoding).
int regNumber(const std::string &lower) {
    if (lower.size() == 2 && lower[0] == 'r' && lower[1] >= '0' && lower[1] <= '3') return lower[1] - '0';
    if (lower == "a0") return 4;
    if (lower == "a1") return 5;
    if (lower == "sp") return 6;
    if (lower == "fb") return 7;
    return -1;
}

Operand buildOperand(PccAsmParser::OperandContext *ctx) {
    if (ctx->LBRACKET()) {
        // The one bracketed memory-operand shape this dialect
        // supports - `[fb+N]`/`[fb-N]` (see Operand's own header
        // comment) - classified here by finding the one syllable
        // that names a register among the bracket's own summed
        // expression; every other syllable (there's always exactly
        // one more, the displacement) sums into `expr`, same as
        // every other multi-syllable expression elsewhere in this
        // file. Every other address through a register (array/
        // pointer dereference, `*`/`&`) still goes through the
        // dedicated `ld`/`st` mnemonics instead (see M16cCodegen.h).
        std::vector<Syllable> syls = exprToSyllables(ctx->expr());
        int baseReg = -1;
        size_t baseIdx = 0;
        for (size_t i = 0; i < syls.size(); i++) {
            if (syls[i].kind != SylKind::Symbol) continue;
            if (int r = regNumber(toLower(syls[i].text)); r >= 0) {
                baseReg = r;
                baseIdx = i;
                break;
            }
        }
        if (baseReg < 0) {
            fail("bracketed memory operand needs a base register, e.g. '[fb-4]' - this target's own dialect "
                 "has no other bracketed addressing mode (see M16cCodegen.h)");
        }
        Operand op;
        op.kind = Operand::Disp;
        op.reg = baseReg;
        for (size_t i = 0; i < syls.size(); i++) {
            if (i != baseIdx) op.expr.push_back(syls[i]);
        }
        if (op.expr.empty()) op.expr.push_back({0, SylKind::Number, "0", {}});
        return op;
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

// A mnemonic's own size suffix (`.w`/`.b`) is purely cosmetic in this
// dialect (every value here is a 16-bit word - see M16cCodegen.h's
// own header comment: no byte-sized operation is ever emitted) - it's
// stripped off before lookup so e.g. "mov.w" and "mov" both resolve to
// the same table entry; nothing here ever actually emits a bare `mov`
// without the suffix, but stripping it unconditionally is simpler than
// asserting it's always present.
std::string stripSizeSuffix(const std::string &mnemonic) {
    auto dot = mnemonic.rfind('.');
    if (dot == std::string::npos) return mnemonic;
    return mnemonic.substr(0, dot);
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
        fail("'='-style equates aren't part of this target's dialect");
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
    line.mnemonic = stripSizeSuffix(mnemonic);
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

// One pending fixup `assembleM16cObject()` (see M16cAssembler.h's own
// header comment) turns into a real ObjRelocation once assembly is
// done - `byteOffset` into `bytes()`, `symbolName` the reference that
// couldn't be resolved locally (or the reserved pseudo-symbol ".."
// for a same-file address's own needed base-shift - see
// M16cAssembler.h's own header comment).
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
            case Line::Dw:
                for (const auto &e : line.exprList) emit16(static_cast<uint16_t>(evalExpr(e)));
                return;
            case Line::Instr:
                encodeInstr(line.mnemonic, line.operands);
                return;
        }
    }

    // `externalName`/`needsBaseShift` mirror
    // ../z80/Z80Assembler.cpp's own `evalExpr` exactly - see
    // M16cAssembler.h's own header comment on why *any* local address
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

    void emit8(uint8_t b) {
        bytes_.push_back(b);
        offset_ += 1;
    }

    void emit16(uint16_t w) {
        emit8(static_cast<uint8_t>(w & 0xFF));
        emit8(static_cast<uint8_t>((w >> 8) & 0xFF));
    }

    // The "evalExpr, then emit16 the result" pattern every absolute-
    // address field (a `Mem`-kind operand in `encodeOperand()`, or a
    // jump/call/`lea`'s own address field) shares - see
    // ../z80/Z80Assembler.cpp's own `emit16` for the identical
    // pattern this is ported from. Records a relocation against the
    // reserved pseudo-symbol ".." when the expression touched a local
    // address, and/or a real relocation against an external symbol
    // when it referenced one - both can apply to the same field at
    // once.
    void emitAddr16(const std::vector<Syllable> &expr) {
        std::string externalName;
        bool needsBaseShift = false;
        uint32_t v = evalExpr(expr, &externalName, &needsBaseShift);
        if (stage_ == 2 && allowExternal_) {
            if (needsBaseShift) relocations_.push_back({offset_, ".."});
            if (!externalName.empty()) relocations_.push_back({offset_, externalName});
        }
        emit16(static_cast<uint16_t>(v));
    }

    // --- operand encoding --------------------------------------------
    // This dialect's own invented, simply-decodable operand shape -
    // see M16cAssembler.h's own header comment: not real M16C object
    // code. A one-byte tag (0=register, 1=immediate, 2=absolute
    // address, 3=frame-base-relative) followed by that mode's own
    // value bytes: one byte for a register code, two little-endian
    // bytes for an immediate/absolute-address word, or (for `Disp`)
    // one base-register byte then two little-endian displacement
    // bytes.
    static constexpr uint8_t kTagReg = 0, kTagImm = 1, kTagMem = 2, kTagDisp = 3;

    void encodeOperand(const Operand &op) {
        switch (op.kind) {
            case Operand::Reg:
                emit8(kTagReg);
                emit8(static_cast<uint8_t>(op.reg));
                return;
            case Operand::Imm:
                emit8(kTagImm);
                emit16(static_cast<uint16_t>(evalExpr(op.expr)));
                return;
            case Operand::Mem:
                emit8(kTagMem);
                emitAddr16(op.expr);
                return;
            case Operand::Disp:
                // Never an address needing a relocation (a frame
                // displacement is a pure compile-time integer, always
                // resolved relative to a runtime register value, never
                // to this program's own load address) - a plain
                // evalExpr, same as Imm's.
                emit8(kTagDisp);
                emit8(static_cast<uint8_t>(op.reg));
                emit16(static_cast<uint16_t>(evalExpr(op.expr)));
                return;
        }
    }

    static const Operand &requireOperand(const std::vector<Operand> &ops, size_t i, const std::string &mnemonic) {
        if (ops.size() <= i) fail("'" + mnemonic + "': missing operand");
        return ops[i];
    }
    static void requireOperandCount(const std::vector<Operand> &ops, size_t want, const std::string &mnemonic) {
        if (ops.size() != want) {
            fail("'" + mnemonic + "' needs exactly " + std::to_string(want) + " operand(s)");
        }
    }
    static uint8_t requireReg(const Operand &op, const std::string &mnemonic) {
        if (op.kind != Operand::Reg) fail("'" + mnemonic + "' needs a register operand");
        return static_cast<uint8_t>(op.reg);
    }
    // `mul`/`divu`'s own destination is always exactly R0 (see
    // M16cCodegen.h's own header comment) - still spelled out as an
    // ordinary operand in the mnemonic/codegen source for clarity, but
    // validated (and not separately encoded, since it can only ever
    // be one value) rather than genuinely variable.
    static void requireR0(const Operand &op, const std::string &mnemonic) {
        if (op.kind != Operand::Reg || op.reg != 0) fail("'" + mnemonic + "': destination must be r0");
    }

    // The two-operand "dst = dst OP src" family (mov/add/sub/and/or/
    // xor/shl/cmp - see M16cCodegen.h's own header comment on this
    // convention; `mov` is the one exception, "dst = src"). `writesDst`
    // rejects an immediate destination for every member that actually
    // writes one; `cmp` is the one exception (read-only).
    void encodeTwoOp(uint8_t opcode, const std::string &mnemonic, const std::vector<Operand> &ops, bool writesDst) {
        requireOperandCount(ops, 2, mnemonic);
        const Operand &src = ops[0];
        const Operand &dst = ops[1];
        if (writesDst && dst.kind == Operand::Imm) fail("'" + mnemonic + "': an immediate can't be a destination");
        emit8(opcode);
        encodeOperand(src);
        encodeOperand(dst);
    }

    // The one-operand family (not/neg - both write their own operand
    // in place, so an immediate is never valid there either).
    void encodeOneOp(uint8_t opcode, const std::string &mnemonic, const std::vector<Operand> &ops) {
        requireOperandCount(ops, 1, mnemonic);
        if (ops[0].kind == Operand::Imm) fail("'" + mnemonic + "': an immediate can't be a destination");
        emit8(opcode);
        encodeOperand(ops[0]);
    }

    // The full instruction vocabulary ../M16cCodegen.cpp ever emits -
    // see M16cAssembler.h's own header comment. Not a general M16C
    // encoder: exactly these mnemonic/operand-shape combinations.
    void encodeInstr(const std::string &mnemonic, const std::vector<Operand> &ops) {
        static const std::unordered_map<std::string, uint8_t> twoOpWrite = {
            {"mov", 0x01}, {"add", 0x02}, {"sub", 0x03}, {"and", 0x04}, {"or", 0x05}, {"xor", 0x06}, {"shl", 0x07},
        };
        if (auto it = twoOpWrite.find(mnemonic); it != twoOpWrite.end()) {
            encodeTwoOp(it->second, mnemonic, ops, /*writesDst=*/true);
            return;
        }
        if (mnemonic == "cmp") {
            encodeTwoOp(0x08, mnemonic, ops, /*writesDst=*/false);
            return;
        }
        static const std::unordered_map<std::string, uint8_t> oneOp = {
            {"not", 0x09},
            {"neg", 0x0A},
        };
        if (auto it = oneOp.find(mnemonic); it != oneOp.end()) {
            encodeOneOp(it->second, mnemonic, ops);
            return;
        }
        if (mnemonic == "mul" || mnemonic == "divu") {
            requireOperandCount(ops, 2, mnemonic);
            requireR0(ops[1], mnemonic);
            emit8(mnemonic == "mul" ? 0x0B : 0x0C);
            encodeOperand(ops[0]);
            return;
        }
        if (mnemonic == "push") {
            requireOperandCount(ops, 1, mnemonic);
            uint8_t rn = requireReg(ops[0], mnemonic);
            emit8(0x0D);
            emit8(rn);
            return;
        }
        if (mnemonic == "pop") {
            requireOperandCount(ops, 1, mnemonic);
            uint8_t rn = requireReg(ops[0], mnemonic);
            emit8(0x0E);
            emit8(rn);
            return;
        }
        // `ld addrReg,dst` / `st src,addrReg` - the dedicated
        // register-indirect-through-an-address-register mnemonics (see
        // M16cCodegen.h's own header comment on why there's no
        // bracketed-operand syntax instead).
        if (mnemonic == "ld") {
            requireOperandCount(ops, 2, mnemonic);
            uint8_t addr = requireReg(ops[0], mnemonic);
            uint8_t dst = requireReg(ops[1], mnemonic);
            emit8(0x0F);
            emit8(addr);
            emit8(dst);
            return;
        }
        if (mnemonic == "st") {
            requireOperandCount(ops, 2, mnemonic);
            uint8_t src = requireReg(ops[0], mnemonic);
            uint8_t addr = requireReg(ops[1], mnemonic);
            emit8(0x10);
            emit8(src);
            emit8(addr);
            return;
        }
        if (mnemonic == "lea") {
            requireOperandCount(ops, 2, mnemonic);
            // Source is either an absolute label (`Mem`, a Global's
            // own address) or a frame-relative `[fb+N]`/`[fb-N]`
            // (`Disp`, a Local/Param's own address - see
            // M16cCodegen.h's `frameOperand()`) - every other kind is
            // never a real address. Checked explicitly against both
            // allowed kinds (not just "!= Mem") so a future third
            // memory-operand shape can't silently fall through this
            // check the way an exclusion list checked against only
            // the OLD shapes would.
            if (ops[0].kind != Operand::Mem && ops[0].kind != Operand::Disp) {
                fail("'lea': source must be a label or a frame-relative '[fb+N]'/'[fb-N]' address");
            }
            uint8_t dst = requireReg(ops[1], mnemonic);
            emit8(0x11);
            encodeOperand(ops[0]);
            emit8(dst);
            return;
        }
        if (mnemonic == "jsr") {
            requireOperandCount(ops, 1, mnemonic);
            if (ops[0].kind != Operand::Mem) fail("'jsr': target must be a label");
            emit8(0x12);
            emitAddr16(ops[0].expr);
            return;
        }
        // `JSRI.W` - the real M16C indirect-call instruction: unlike
        // `jsr`, whose Mem operand's own address IS the subroutine
        // entry point, `jsri`'s operand names the memory location
        // *holding* the entry point - the word stored there becomes
        // the new PC (after the return address is pushed, same as
        // `jsr`). The real hardware form supports a general EA for
        // that memory location (register-relative, absolute, etc.);
        // this dialect only ever needs the absolute-address case
        // (M16cCodegen.cpp's own genExpr() always spills the callee's
        // address to a temp word cell first, then points `jsri` at
        // that cell's own label - see that file's own CallPtr
        // comment).
        if (mnemonic == "jsri") {
            requireOperandCount(ops, 1, mnemonic);
            if (ops[0].kind != Operand::Mem) fail("'jsri': target must be a label");
            emit8(0x1D);
            emitAddr16(ops[0].expr);
            return;
        }
        if (mnemonic == "rts") {
            requireOperandCount(ops, 0, mnemonic);
            emit8(0x13);
            return;
        }
        if (mnemonic == "jmp") {
            requireOperandCount(ops, 1, mnemonic);
            if (ops[0].kind != Operand::Mem) fail("'jmp': target must be a label");
            emit8(0x14);
            emitAddr16(ops[0].expr);
            return;
        }
        static const std::unordered_map<std::string, uint8_t> condJump = {
            {"jeq", 0x15}, {"jne", 0x16}, {"jlt", 0x17}, {"jle", 0x18}, {"jgt", 0x19}, {"jge", 0x1A},
        };
        if (auto it = condJump.find(mnemonic); it != condJump.end()) {
            requireOperandCount(ops, 1, mnemonic);
            if (ops[0].kind != Operand::Mem) fail("'" + mnemonic + "': target must be a label");
            emit8(it->second);
            emitAddr16(ops[0].expr);
            return;
        }
        if (mnemonic == "fset" || mnemonic == "fclr") {
            // `fset i` / `fclr i` - the interrupt-enable flag bit is
            // this dialect's own only ever operand to either mnemonic
            // (see M16cCodegen.cpp's own Intrinsic case); `i` is just
            // a bare identifier to the shared grammar, consumed here
            // without needing its own register/operand classification.
            requireOperandCount(ops, 1, mnemonic);
            if (toLower(ops[0].expr.empty() ? "" : ops[0].expr[0].text) != "i" || ops[0].kind != Operand::Mem) {
                fail("'" + mnemonic + "' only supports the 'i' (interrupt enable) flag");
            }
            emit8(mnemonic == "fset" ? 0x1B : 0x1C);
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
// Assembler produced - see M16cAssembler.h's own `assembleM16cObject`
// comment, and ../z80/Z80Assembler.cpp's own `buildObjectFile` for
// the identical algorithm this is ported from.
ObjectFile buildObjectFile(const Assembler &a) {
    ObjectFile obj;
    obj.machineType = kM16cCoffMachine;
    const auto &bytesVec = a.bytes();
    obj.textBytes.assign(bytesVec.begin(), bytesVec.end());

    for (const auto &kv : a.labels()) {
        obj.symbols.push_back({kv.first, kv.second - a.orgBase(), /*isDefined=*/true});
    }
    std::sort(obj.symbols.begin(), obj.symbols.end(),
              [](const ObjSymbol &x, const ObjSymbol &y) { return x.name < y.name; });

    // Every relocation's target symbol needs a symbol-table entry
    // too - add one, undefined, for any name that isn't already a
    // real local label (this also covers the reserved pseudo-symbol
    // ".." - a linker resolves that one specially, never by looking
    // it up here, but it still needs a table entry for the object-
    // format writer to point the relocation's own symbol index at).
    std::unordered_map<std::string, bool> known;
    for (const auto &s : obj.symbols) known[s.name] = true;
    for (const auto &r : a.relocations()) {
        if (!known.count(r.symbolName)) {
            obj.symbols.push_back({r.symbolName, 0, /*isDefined=*/false});
            known[r.symbolName] = true;
        }
        obj.relocations.push_back({r.byteOffset, r.symbolName, kM16cRelocAbs16});
    }
    return obj;
}

}  // namespace

std::string assembleM16c(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/false);
    assembler.run(lines);
    const auto &bytes = assembler.bytes();
    return std::string(bytes.begin(), bytes.end());
}

ObjectFile assembleM16cObject(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/true);
    assembler.run(lines);
    return buildObjectFile(assembler);
}

}  // namespace pcc
