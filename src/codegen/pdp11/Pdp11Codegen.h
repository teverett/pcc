#pragma once

#include <map>
#include <optional>
#include <sstream>
#include <utility>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace pcc {

// Generates PDP-11 assembly (this project's own dialect - see Pdp11Assembler.h) for the IR
// built by IRBuilder. `generate()` returns a complete flat binary loaded at a fixed address
// (`kBaseAddress`) with no OS/boot loader; it halts via `HALT` with a `br` safety net after
// it in case a simulator just resumes instead of stopping.
//
// This language's `int` is 16 bits (the PDP-11's native word size), so no byte-sized (`.B`)
// instruction is ever emitted for an `int`. Live values use three general registers: `R0`
// (genExpr()'s result), `R1` (the other operand of a binary op), `R2` (a computed address
// when dereferencing, via the PDP-11's real register-deferred `(Rn)` mode). `R5` is the
// dedicated frame-pointer register (see addr()). `SP` (`R6`) is the real hardware stack
// pointer: calls use real `JSR`/`RTS`; every prologue/epilogue pushes/pops the caller's `R5`
// on it; the caller pushes arguments before `jsr` and pops them after return. Every call gets
// its own frame, so recursion is safe.
//
// `*`/`/` use the EIS `MUL`/`DIV`: `MUL src,R0` leaves a 32-bit product in `R0:R1` (the low
// word, `R1`, is the truncated answer); `DIV src,R0` divides the 32-bit dividend in `R0:R1`
// by a 16-bit divisor, so the dividend is first sign-extended into that pair with `SXT`.
//
// `<<`/`>>` have no variable-count hardware shift usable here (EIS `ASH` shifts
// arithmetically, which would make `>>` sign-extend unlike every other backend's logical
// convention), so both loop one bit at a time: left shift is `ASL`; right shift is `CLC`
// then `ROR` (rotates a cleared carry into bit 15, instead of `ASR`'s sign-bit fill).
//
// Comparisons use `CMP` + a signed branch directly (PDP-11 condition codes already encode
// signed comparisons correctly). `CMP src,dst` sets flags from `src - dst`, so `CMP R1,R0`
// (R1=lhs, R0=rhs) computes `lhs - rhs` with no operand-order inversion needed.
//
// Every branch here is a short branch to an immediately-following label plus a full-range
// `JMP` (see `safeBranch()`), since the PDP-11's branch instructions can only reach ±128
// words from the next instruction.
class Pdp11Codegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "pdp11"; }
    TypeWidths typeWidths() const override { return {1, 2, 2, 4, 8}; }
    std::string sourceExtension() const override { return ".s11"; }
    std::vector<std::string> assembledFormats() const override {
        return {"raw", "coff", "elf", "macho", "aout", "objraw"};
    }
    // `raw` is a whole-program flat binary; the other five are relocatable object formats.
    bool isObjectFormat(const std::string &format) const override { return format != "raw" && format != "asm"; }
    std::string defaultAssemblerPath() const override { return ""; }
    std::string buildAssembleCommand(const std::string &assemblerPath, const std::string &format,
                                      const std::string &srcPath, const std::string &outPath) const override;
    std::map<std::string, std::string> perFileAsm() const override;
    std::optional<std::string> assembleInProcess(const std::string &asmText,
                                                  const std::string &format) const override;

   private:
    void genFunction(const ir::FunctionBody &fn);
    void genStmt(const ir::Stmt &s);
    void genExpr(const ir::Expr &e);  // leaves the result in R0

    void genBinary(const ir::Expr &e);
    void genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned);
    void emitCompare(const ir::Expr &lhs, const ir::Expr &rhs, const std::string &branchIfTrue);
    void genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalNot(const ir::Expr &rhs);
    // After genExpr(e), leaves r0 holding 0/nonzero and `TST r0`-ready for a branch; for a
    // wide value (left as `words` words on the stack), ORs them all into r0 first.
    void testTruthy(const ir::Expr &e);
    void genMul(const ir::Expr &lhs, const ir::Expr &rhs);
    // Unsigned 16-bit divide: r1 = dividend, r2 = divisor on entry;
    // r0 = quotient, r1 = remainder on exit. Clobbers only r0-r2.
    void emitUnsignedDiv();
    void genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned);
    void genShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned);
    void genIndex(const ir::Expr &e);         // slot[idx] read
    void genAddrOf(const ir::Expr &operand);  // &slot or &slot[idx]
    void genLocalArrayInit(const ir::Slot *slot);  // stores arrayInit into a Local's own frame slots
    void genConvert(const ir::Expr &e);
    // CompoundAssign/CompoundDerefAssign's "new = old <op> rhs" step - `old` in r0, `rhs` in
    // r1 (both at `type`'s width/signedness, never wide here), leaves the raw new value in
    // r0 (see narrowR0To()).
    void genCompoundOp(const std::string &op, Type type);
    // Shared tail for CompoundAssign's indexed form and CompoundDerefAssign - r2 must already
    // hold the resolved address on entry.
    void genCompoundThroughR2(const ir::Expr &e);
    // Re-narrows r0 from its low byte, per `to`'s signedness, when `to` is a `char`; a no-op
    // otherwise.
    void narrowR0To(Type to);

    int widthOfType(Type t) const { return pcc::widthOf(t, typeWidths()); }
    bool isWide(Type t) const { return widthOfType(t) > widthOfType(Type{BaseType::Int, false}); }
    // How many native 16-bit words `t` occupies - 1 up to `int`, 2 for `long`, 4 for `long long`.
    int wordCountOf(Type t) const { return widthOfType(t) / 2; }
    // log2 of the element width (1/2/4/8 bytes -> 0/1/2/3), the shift amount genIndexAddr()
    // scales an index by. Note: a `long`/`long long` array element still only moves one
    // native word (see genIndex()).
    int elemShift(Type t) const {
        switch (widthOfType(t)) {
            case 1: return 0;
            case 2: return 1;
            case 4: return 2;
            default: return 3;
        }
    }

    // Loads a value of `type` from effective-address text `mem` into r0 (a wide type's N
    // words end up on the stack - see genWideLoad()), sign/zero-extending a `char` via
    // `movb`'s hardware auto-sign-extend, masking the high byte afterward when unsigned.
    void loadOperand(const std::string &mem, Type type);
    // Stores r0 into `mem`, truncating to `type`'s width (`char` writes only the low byte
    // via `movb`).
    void storeOperand(const std::string &mem, Type type);

    // Register-indirect counterpart of loadOperand(), for a dereferenced pointer whose
    // effective address is already a runtime value in `addrReg`. `ldind` only addresses a
    // bare `(Rn)`, so a wide pointee instead uses the general `[Rn+N]` indexed form to reach
    // words 1, 2, .... Leaves r0 (narrow) or pushes `wordCountOf(type)` words (wide, least
    // significant on top).
    void loadIndirect(const std::string &addrReg, Type type);
    // `*lhs = rhs`'s store: `r2` already holds `lhs`'s pointer value pushed on the stack, and
    // `rhs`'s value is evaluated on top of it. Pops everything, recovering the address last
    // (underneath a wide rhs's pushed words) and writing through it at `type`'s width.
    void storeIndirect(Type type);

    // `long`/`long long` arithmetic: a value of word count N lives on the stack as N
    // consecutive words, word 0 (least significant) closest to `sp`. Every wide helper below
    // takes `words` (N) explicitly, serving both `long` (2) and `long long` (4).
    void genWideAdd(const ir::Expr &lhs, const ir::Expr &rhs, bool isAdd, int words);
    void genWideMul(const ir::Expr &lhs, const ir::Expr &rhs, int words);
    void genWideDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned, int words);
    void genWideShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned, int words);
    void genWideCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned, int words);
    void genWideBitwise(const ir::Expr &lhs, const ir::Expr &rhs, const std::string &op, int words);
    // Negates the `words`-word value already on top of the stack, in place.
    void genWideNegate(int words);
    // Loads `slot`'s `words`-word value onto the stack (word 0 at the new top).
    void genWideLoad(const ir::Slot *slot, int words);
    void genWideLoadLiteral(long value, int words);
    // Pops `words` words off the stack into `slot`'s own storage.
    void genWideStore(const ir::Slot *slot, int words);
    // Word `wordIndex` (0 = least significant) of `slot`'s multi-word storage; each later
    // word sits 2 bytes further from `r5`/the label.
    std::string wideWordOperand(const ir::Slot *slot, int wordIndex) const;

    // Leaves slot[idxExpr]'s effective address in `R2`.
    // How far below `r5` a Local array's lowest address (element 0) sits.
    int localArrayBase(const ir::Slot *slot);
    void genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr);

    // Encodes "if the branch condition holds, jump to `targetIfTaken`, else `targetIfNot`"
    // as a short branch to an immediately-following label (within ±128 words) followed by an
    // unconditional full-range `jmp`.
    void safeBranch(const std::string &branchMnemonic, const std::string &targetIfTaken,
                     const std::string &targetIfNot);

    // Only a Global/Function gets a name-based label; a Local/Param is addressed relative to
    // `R5` instead (see addr()).
    static std::string label(const ir::Slot *slot) { return "g_" + slot->sourceName; }
    static std::string label(const ir::Function *fn) { return "fn_" + fn->sourceName; }

    // A Local/Param's frame-pointer-relative address text, e.g. "[r5-4]"/"[r5+6]". After
    // genFunction()'s prologue, `r5` points at this call's saved old `r5`, with the return
    // address one word above it and the caller's pushed arguments above that (param 0
    // closest to the return address), so a Param's address is `r5+frameOffset`. Locals live
    // below `r5`, at ir::localOffsetDescendingWide() (a wide value's later words further
    // below, see wideWordOperand()).
    std::string addr(const ir::Slot *slot) const {
        if (slot->kind == ir::SlotKind::Param) {
            return "[r5+" + std::to_string(slot->frameOffset) + "]";
        }
        return "[r5-" + std::to_string(ir::localOffsetDescendingWide(*slot, typeWidths())) + "]";
    }

    // The operand text for a plain read/write of `slot`'s value - the bare label for a
    // Global, or addr(slot) ("[r5±N]") for a Local/Param.
    std::string operandOf(const ir::Slot *slot) const {
        return slot->kind == ir::SlotKind::Global ? label(slot) : addr(slot);
    }

    std::string allocTemp() { return "t" + std::to_string(tempCounter_++); }
    void declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values, Type elemType);
    std::string arrayElemText(const ir::ArrayInitElem &e);
    std::string newLabel() { return "L" + std::to_string(labelCounter_++); }

    std::ostringstream code_;
    std::vector<std::string> trailer_;  // "label: dw v0, v1, ..." data lines, emitted once at the end
    std::map<std::string, std::string> perFileAsm_;

    int tempCounter_ = 0;
    int labelCounter_ = 0;
    // One entry per currently-open loop (innermost last): {continue label, break label}.
    std::vector<std::pair<std::string, std::string>> loopLabels_;
};

}  // namespace pcc
