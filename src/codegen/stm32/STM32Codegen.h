#pragma once

#include <map>
#include <optional>
#include <sstream>
#include <utility>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace pcc {

// Generates Thumb-2 assembly (see STM32Assembler.h) for a real STM32 Cortex-M3.
//
// `int` is 32 bits, the native register width. Live values use four registers: r0 (genExpr()'s
// result), r1 (the other operand of a binary op), r2 (a computed address), r3 (scratch for a
// frame-relative offset, see loadFrameWordAddr()). r7 is the dedicated frame pointer (see
// genFunction()), never used otherwise. The hardware stack saves one operand across evaluating
// the other, passes call arguments, and reserves Local frame storage; calls use real
// `bl`/`push {lr}`+`pop {pc}` linkage.
//
// Each function's frame (see genFunction()): `pushlr` saves the return address, `push r7` saves
// the caller's frame pointer, `mov r7,sp`, then one dummy `push r0` per word of
// Function::frameSize reserves Local storage (only the resulting sp displacement matters - this
// dialect has no `sub sp,#imm`). emitEpilogue() reverses this. A Param's address is
// `r7 + 8 + 4*paramIndex` (past the saved r7 and return address); a Local's is
// `r7 - 4*frameOffset` (growing the opposite direction, so a Local array's element i sits at
// address base-minus-4*i, not base-plus - see genIndexAddr()).
//
// Call arguments are pushed by the caller, last-declared one first, so the first argument ends
// up closest to the return address (matching Param 0's r7+8). The caller pops them back off
// afterward with one dummy `pop r1` per argument.
//
// `*` uses hardware `muls` (full 32x32->32). `/`/`%` use hardware `sdiv`/`udiv` - no
// software-loop or operand-width caveat needed, unlike most other backends. `<<`/`>>` use the
// hardware variable-count shift; `>>` is logical/unsigned by this codebase's usual convention.
// Comparisons use `cmp` + a condition-code branch; flags already encode signed ordering
// correctly. Every branch always uses the 32-bit Thumb-2 "wide" encoding (~1MB range), so no
// branch-range workaround is needed.
//
// Flash and RAM are two separate, non-overlapping address ranges (flash is real read-only ROM
// on this board), so generate() emits two regions: flash (kFlashBase: vector table + every
// instruction) and RAM (kRamBase: every global/local/temp/array slot, reserved not
// initialized-in-place). Since RAM starts unknown, any global with a nonzero initial value
// needs an explicit copy, emitted as flash-resident code that runs once before `main`.
class STM32Codegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "stm32"; }
    TypeWidths typeWidths() const override { return {1, 2, 4, 4, 8}; }
    std::string sourceExtension() const override { return ".s32"; }
    std::vector<std::string> assembledFormats() const override {
        return {"raw", "coff", "elf", "macho", "aout", "objraw"};
    }
    // `raw` is a whole-program flat binary (vector table included); the other five are
    // relocatable object formats for separate compilation.
    bool isObjectFormat(const std::string &format) const override { return format != "raw"; }
    std::string defaultAssemblerPath() const override { return ""; }
    std::string buildAssembleCommand(const std::string &assemblerPath, const std::string &format,
                                      const std::string &srcPath, const std::string &outPath) const override;
    std::map<std::string, std::string> perFileAsm() const override;
    std::optional<std::string> assembleInProcess(const std::string &asmText,
                                                  const std::string &format) const override;

   private:
    void genFunction(const ir::FunctionBody &fn);
    void genStmt(const ir::Stmt &s);
    void genExpr(const ir::Expr &e);  // leaves the result in r0

    void genBinary(const ir::Expr &e);
    void genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs);
    void emitCompare(const ir::Expr &lhs, const ir::Expr &rhs, const std::string &branchIfTrue, bool isUnsigned);
    void genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalNot(const ir::Expr &rhs);
    void genMul(const ir::Expr &lhs, const ir::Expr &rhs);
    void genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned);
    void genIndex(const ir::Expr &e);         // slot[idx] read
    void genAddrOf(const ir::Expr &operand);  // &slot or &slot[idx]
    void genLocalArrayInit(const ir::Slot *slot);  // stores arrayInit into a Local's frame slots
    void genConvert(const ir::Expr &e);
    // CompoundAssign/CompoundDerefAssign's "new = old <op> rhs": old in r0, rhs in r1, leaves
    // the raw (not yet renarrowed) result in r0. `type` is never wide here.
    void genCompoundOp(const std::string &op, Type type);
    // Shared tail for CompoundAssign's indexed form and CompoundDerefAssign - r2 must already
    // hold the resolved address on entry and is preserved across evaluating e.rhs.
    void genCompoundThroughR2(const ir::Expr &e);

    // Leaves slot[idxExpr]'s effective address in `r2`.
    void genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr);

    int widthOfType(Type t) const { return pcc::widthOf(t, typeWidths()); }
    // Only `long long` is wider than the native 32-bit int - the one type needing the r0:r1
    // register-pair convention below.
    bool isWide(Type t) const { return widthOfType(t) > widthOfType(Type{BaseType::Int, false}); }
    // log2 of an element type's width (1/2/4/8 bytes -> 0/1/2/3), the shift genIndexAddr() uses.
    int elemShift(Type t) const {
        switch (widthOfType(t)) {
            case 1: return 0;
            case 2: return 1;
            case 4: return 2;
            default: return 3;
        }
    }

    // Loads a value of `type` from the address in `addrReg` into r0 (or r0:r1 for `long long`),
    // sign- or zero-extending per signedness. `addrReg` must not be r0/r1.
    void loadOperand(const std::string &addrReg, Type type);
    // Stores r0 (or r0:r1, for `long long`) into `addrReg`, truncating to `type`'s width.
    void storeOperand(const std::string &addrReg, Type type);
    // Re-extends r0 from its low widthOf(to) bytes via `lsl`+`asr`/`lsr` (no dedicated
    // sign/zero-extend instruction here).
    void narrowR0To(Type to);

    // `long long` add/sub: ripples carry/borrow across the word pair with `adc`/`sbc`.
    void genWideAdd(const ir::Expr &lhs, const ir::Expr &rhs, bool isAdd);
    // `long long` multiply, keeping the low 64 bits of the true 128-bit product. No widening
    // 32x32->64 multiply instruction, so the low*low term is built via genMul64()'s bit-serial
    // shift-and-add loop.
    void genWideMul(const ir::Expr &lhs, const ir::Expr &rhs);
    // 32x32->64 unsigned widening multiply: {r1:r0} = r6(a) * r4(b), both consumed. Clobbers r0-r6.
    void genMul64();
    // Software 64-bit divide/modulo (no instruction here is wide enough): bit-serial
    // shift/compare/subtract long division.
    void genWideDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned);
    // A `long long` shifted by a runtime count: a counted loop of 1-bit shifts, since the
    // hardware variable-count shift only operates within one 32-bit register.
    void genWideShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned);
    void genWideCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned);
    // Negates the 64-bit value already in r0:r1, in place.
    void genNegate64();

    // Emits flash-resident startup code that copies `value` into the RAM address `label(slot)`
    // resolves to, since RAM content can't be preset in the image.
    void genGlobalInit(const std::string &ramLabel, Type type, long value);

    // Emits the epilogue needed at every `return` and at fall-off-the-end: pops reserved Local
    // words, restores the caller's r7, then `poplr`.
    void emitEpilogue();

    // Loads `r7 + byteOffsetFromFp` into `reg` (negative uses `sub`). Uses r3 as scratch, never
    // as `reg` itself.
    void loadFrameWordAddr(const std::string &reg, int byteOffsetFromFp);
    // Loads a Local or Param Slot's effective address into `reg`: a Param's is
    // `r7 + 8 + 4*paramIndex`; a Local's is `r7 - 4*frameOffset`. Never called for a Global.
    void loadFrameAddr(const std::string &reg, const ir::Slot *slot);
    // Loads a Slot's effective address into `reg`: `lea` for Global, loadFrameAddr() otherwise.
    void loadSlotAddr(const std::string &reg, const ir::Slot *slot);
    // Loads one ir::ArrayInitElem's value into `reg` at runtime (literal via `movl`, symbol
    // address via `lea`, including the Function case's thumb-bit adjustment) - used by
    // genLocalArrayInit() where genArrayElemInit() only targets a Global's static RAM word.
    void loadArrayInitElem(const std::string &reg, const ir::ArrayInitElem &e);

    static std::string label(const ir::Slot *slot) {
        if (slot->kind == ir::SlotKind::Global) return "g_" + slot->sourceName;
        if (slot->kind == ir::SlotKind::Param) {
            return "p_" + slot->ownerFunctionName + "_" + std::to_string(slot->paramIndex);
        }
        return slot->sourceName + "_" + std::to_string(slot->id);
    }
    static std::string label(const ir::Function *fn) { return "fn_" + fn->sourceName; }

    std::string allocTemp() { return "t" + std::to_string(tempCounter_++); }
    // Reserves one RAM word for a Global scalar slot (`ds`). A Local/Param's storage lives in
    // its own stack frame instead.
    void declareTemp(const std::string &lbl, Type type);
    // Reserves `widthOfType(elemType)*values.size()` RAM bytes (`dsb`) for a Global array slot,
    // and emits a startup copy for every nonzero element. Never called for a Local array.
    void declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values, Type elemType);
    // Flash-resident code writing one array element into the RAM cell at `lbl+offset` (bytes).
    // Only used for a Global array's static initializer; see genLocalArrayInit() for Locals.
    void genArrayElemInit(const std::string &lbl, size_t offset, Type elemType, const ir::ArrayInitElem &e);
    std::string newLabel() { return "L" + std::to_string(labelCounter_++); }

    std::ostringstream code_;       // flash: vector table + _start + every function body
    std::ostringstream initCode_;   // flash: global-init copies, spliced into _start before `bl main`
    std::vector<std::string> ramTrailer_;  // RAM: "label: ds N" reservation lines, emitted once at the end
    std::map<std::string, std::string> perFileAsm_;

    int tempCounter_ = 0;
    int labelCounter_ = 0;
    // The current function's Function::frameSize, read by emitEpilogue() at every `return`.
    int frameSize_ = 0;
    // One entry per open loop (innermost last): {continue label, break label}.
    std::vector<std::pair<std::string, std::string>> loopLabels_;
};

}  // namespace pcc
