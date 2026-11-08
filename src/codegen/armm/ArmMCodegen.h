#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <sstream>
#include <utility>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace pcc {

// Generates ARM Thumb assembly (this project's dialect - see ArmMAssembler.h)
// targeting ARMv7-M mainline (Cortex-M3/M4/M7); emits only real Thumb/Thumb-2
// so it runs unchanged on any ARMv7-M core. Baseline M-profile (ARMv6-M) is
// not covered: it lacks MOVW/MOVT (needed here to load any 32-bit constant)
// and hardware SDIV (unused here regardless, see genDivMod()).
//
// generate() returns a complete self-contained program loaded at a fixed
// address (kBaseAddress) with no vector table or reset exception - the
// simulator sets the PC there directly - halting in a self-branch loop.
//
// `int` is 32 bits. Register convention (not a real ARM calling convention,
// except r7/sp/lr which form a genuine per-call stack frame):
//   r0 - expression accumulator; also MULS's destination and where a return
//        value would sit under the real ABI
//   r1 - the other operand in a binary op; also the discard register for
//        epilogue/call-cleanup pop-only-to-move-sp
//   r2 - holds a computed address when dereferencing (indexing, */&, any
//        frame-relative address)
//   r3 - scratch for comparisons, genDivMod()'s loop, and frame-offset math
//   r7 - frame pointer: prologue points it at the saved-r7 word: every
//        Local/Param address is relative to it, giving real per-call
//        recursion (each nested activation gets its own r7/frame on the
//        hardware stack)
//   sp - real hardware stack pointer; PUSH/POP one register at a time (no
//        {reglist} syntax here - see ArmMAssembler.h); used to save an
//        operand across evaluating the other side of a binary expression,
//        pass arguments, reserve Local storage, and (via pushlr/poppc) for
//        every call/return
//   lr  - real link register; every function pushlr's it on entry and
//        poppc's (POP {pc}) to return - required since ARM has one link
//        register and a nested call would clobber it otherwise. Because
//        pushlr/poppc are ordinary sp-relative push/pop, nested calls stay
//        reentrant with no frame-pointer involvement.
//
// Stack frame (genFunction()): pushlr, push r7, mov r7,sp, then one dummy
// push per word of Function::frameSize to reserve Local storage (only the sp
// displacement matters). emitEpilogue() undoes this in reverse. A Param's
// address is r7+8+4*paramIndex (past saved r7 and return address); a Local's
// is its lowest address (see ir::localLowOffset()), so an array's element i
// is at base-plus-4*i - see genIndexAddr().
//
// ARM has no absolute-addressing memory operand: every load/store needs a
// base register plus displacement. A Global/array-literal access first loads
// its fixed address into r2 via MOVW+MOVT, then uses ldind/stind r2,r0 (this
// dialect's dedicated LDR/STR-with-zero-displacement mnemonics, since the
// assembler has no [Rn,#imm] syntax at all). A Local/Param's frame-relative
// address is instead computed directly into a register via mov/adds/subs
// (loadFrameAddr()), then read/written the same ldind/stind way.
//
// `*` uses hardware MULS (Thumb-1 2-operand form, available on every
// M-profile core): a full 32x32->32 product.
//
// `/`/`%` are a from-scratch software routine (genDivMod()), not SDIV/UDIV,
// so the same code works on cores lacking that hardware: a repeated-
// subtraction loop on absolute values with the sign reapplied afterward -
// correct but slow, acceptable since test-program operands stay small.
//
// `<<`/`>>` use hardware register-form shifts (LSLS/LSRS Rdn,Rm); `>>` is
// logical (LSRS), never arithmetic. A fixed index*4 scale instead uses the
// immediate form (lsli ...,2).
//
// Comparisons use CMP plus a materializing branch (ARM's condition codes
// already encode signed compares correctly in hardware).
//
// Each conditional branch mnemonic (beq/bne/blt/ble/bgt/bge) expands to two
// real Thumb instructions: a short inverted-condition Bcc (fixed tiny
// displacement, just past the next instruction) followed by a wide
// unconditional B with full +-16MB range - see encodeConditionalBranch().
// This avoids the trickier 32-bit wide conditional-branch encoding, at the
// cost of 2 extra bytes per conditional branch.
class ArmMCodegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "cortex-m3"; }
    TypeWidths typeWidths() const override { return {1, 2, 4, 4, 8}; }
    std::string sourceExtension() const override { return ".sarm"; }
    std::vector<std::string> assembledFormats() const override {
        return {"raw", "coff", "elf", "macho", "aout", "objraw"};
    }
    // `raw` is a whole-program flat binary; every other format is a relocatable object file.
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
    void genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalNot(const ir::Expr &rhs);
    void genMul(const ir::Expr &lhs, const ir::Expr &rhs);
    void genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned);
    // Shared loop: entry convention r1=dividend, r0=divisor, result in r0.
    void genDivModCore(bool wantRemainder, bool isUnsigned);
    void genIndex(const ir::Expr &e);         // slot[idx] read
    void genAddrOf(const ir::Expr &operand);  // &slot or &slot[idx]
    void genLocalArrayInit(const ir::Slot *slot);  // stores arrayInit into a Local's frame slots
    void genConvert(const ir::Expr &e);
    // "new = old <op> rhs": old in r0, rhs in r1, leaves untruncated result in r0.
    void genCompoundOp(const std::string &op, Type type);
    // Shared tail for CompoundAssign/CompoundDerefAssign; r2 must hold the
    // resolved target address on entry.
    void genCompoundThroughR2(const ir::Expr &e);

    void genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr);  // leaves address in r2

    int widthOfType(Type t) const { return pcc::widthOf(t, typeWidths()); }
    // Only long long is wider than this target's 32-bit int/long.
    bool isWide(Type t) const { return widthOfType(t) > widthOfType(Type{BaseType::Int, false}); }
    // log2 of an element type's width; the shift genIndexAddr() scales an index by.
    int elemShift(Type t) const {
        switch (widthOfType(t)) {
            case 1: return 0;
            case 2: return 1;
            case 4: return 2;
            default: return 3;
        }
    }

    // Loads `type` from `addrReg` into r0 (or r0:r1 for long long), sign/zero-
    // extending per its signedness. `addrReg` must not be r0/r1.
    void loadOperand(const std::string &addrReg, Type type);
    // Stores r0 (or r0:r1 for long long) into `addrReg`, truncated to `type`.
    void storeOperand(const std::string &addrReg, Type type);
    // Re-extends r0 from its low widthOf(to) bytes via lsl+asr/lsr: no dedicated
    // sign/zero-extend instruction here.
    void narrowR0To(Type to);

    // long long add/sub: lhs in r0:r1, rhs in r4:r5, carry/borrow rippled
    // across the word pair with adc/sbc.
    void genWideAdd(const ir::Expr &lhs, const ir::Expr &rhs, bool isAdd);
    // long long multiply, low 64 bits of the true 128-bit product. No widening
    // 32x32->64 multiply here, so the low*low term uses genMul64()'s bit-serial
    // loop; the other two cross terms only affect the high word, where muls's
    // truncating result is already correct.
    void genWideMul(const ir::Expr &lhs, const ir::Expr &rhs);
    // 32x32->64 unsigned widening multiply: {r1:r0} = r6 * r4 (caller places
    // operands in r6/r4 first). Bit-serial shift-and-add, 32 iterations; clobbers r0-r6.
    void genMul64();
    // Software 64-bit divide/modulo: bit-serial shift/compare/subtract long
    // division, 64 iterations.
    void genWideDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned);
    // long long shifted by a runtime count: a counted loop of 1-bit shifts
    // carrying the cross-word bit by hand. `isUnsigned` only affects `>>`
    // (asr vs lsr on the high word).
    void genWideShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned);
    void genWideCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned);
    void genNegate64();

    // Epilogue for every `return` and the fall-off-the-end default: pops the
    // reserved Local words (into scratch r1, discarded), restores r7, poppc.
    void emitEpilogue();

    void loadAddress(const std::string &reg, const std::string &lbl);
    // MOVS alone if the constant fits in 8 bits, MOVW+MOVT otherwise.
    void loadImm32(const std::string &reg, uint32_t value);

    // Loads r7 + bytesFromFp into `reg` (subs when negative). Uses r3 as scratch.
    void loadFrameByteAddr(const std::string &reg, int bytesFromFp);
    // Loads a Local/Param's address from slot->frameOffset: a Param's grows up
    // from r7, a Local's grows down. Never called for a Global Slot.
    void loadFrameAddr(const std::string &reg, const ir::Slot *slot);
    void loadSlotAddr(const std::string &reg, const ir::Slot *slot);
    void loadArrayInitElem(const std::string &reg, const ir::ArrayInitElem &e);

    static std::string label(const ir::Slot *slot) {
        if (slot->kind == ir::SlotKind::Global) return "g_" + slot->sourceName;
        if (slot->kind == ir::SlotKind::Param) {
            return "p_" + slot->ownerFunctionName + "_" + std::to_string(slot->paramIndex);
        }
        return slot->sourceName + "_" + std::to_string(slot->id);
    }
    static std::string label(const ir::Function *fn) { return "fn_" + fn->sourceName; }

    void declareTemp(const std::string &lbl);
    void declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values, Type elemType);
    std::string arrayElemText(const ir::ArrayInitElem &e, Type elemType);
    std::string newLabel() { return "L" + std::to_string(labelCounter_++); }

    std::ostringstream code_;
    std::vector<std::string> trailer_;  // "label: dw v0, v1, ..." data lines, emitted once at the end
    std::map<std::string, std::string> perFileAsm_;

    int labelCounter_ = 0;
    // Current function's Function::frameSize, read by emitEpilogue().
    int frameSize_ = 0;
    // {continue label, break label} per open loop, innermost last.
    std::vector<std::pair<std::string, std::string>> loopLabels_;
};

}  // namespace pcc
