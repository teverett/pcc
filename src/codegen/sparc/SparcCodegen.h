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

// Generates 32-bit SPARC V8 assembly for the IR built by IRBuilder. `generate()` returns
// a complete, self-contained program loaded at a fixed address (kBaseAddress) with no OS
// or boot ROM, halting in a self-branch infinite loop.
//
// `int` is 32 bits; `char`/`short` are narrower (see typeWidths()); `long long` is a
// software 64-bit value kept in a register pair (`t1:t0`, high:low).
//
// Real SPARC's windowed `%i`/`%l`/`%o` register banks (rotated by `save`/`restore`) are
// deliberately not used: the simulator has no OS to service a window-overflow trap, so
// relying on hardware windows would cap recursion depth at the implementation's window
// count. Instead `%i`/`%l`/`%o`/`%g` stay plain, fixed physical-register names for the
// whole run, and recursion safety comes from a conventional software stack frame: every
// call gets its own frame on the real hardware stack (`sp`), addressed through an
// explicit frame-pointer register (`fp`) this backend pushes/pops itself (see
// genFunction() for the exact layout). Register roles:
//   t0 (%o0) - "ac", genExpr()'s result; also the real SPARC ABI return-value register.
//              For a `long long`, `t1:t0` (high:low) together.
//   t1 (%o1) - the other operand in a binary op, or a wide value's high word.
//   t2 (%o2) - holds a computed address when dereferencing one (array/pointer indexing,
//              `*`/`&`, or a Local/Param's frame-relative address - see loadSlotAddr()).
//   t3-t6 (%l0-%l3) - scratch registers for the wide (`long long`) helpers below
//              (genWideAdd/genWideMul/genWideShift/genWideCompare), which each juggle two
//              64-bit operands (four 32-bit words) at once.
//   sp (%o6) - the SPARC stack pointer: saves one operand while evaluating the other in
//              a binary expression, pushes call arguments, and (in every prologue/
//              epilogue) pushes/pops `lr`/`fp` and reserves/releases Local storage.
//   lr (%o7) - the register `call` writes the return address into, directly
//              readable/writable like any GPR. Spilled to the frame in the prologue and
//              reloaded in the epilogue (see genFunction()), so a non-leaf function's own
//              calls don't clobber it before its `retl` runs.
//   fp       - this backend's dedicated frame-pointer register (not real SPARC's
//              windowed `%fp`/`%i6`): points at this call's saved-`fp` word, giving every
//              Local/Param a stable per-call address (see loadSlotAddr()).
//   g0       - the hardwired-zero register, used for the `or g0,src,dst` "move register"
//              idiom and as `sdiv`/`call`'s discarded-result register.
//
// SPARC has no absolute-addressing memory operand: every load/store goes through a base
// register plus a displacement (zero for plain `ldind`/`stind`, or a compile-time
// constant third operand - see SparcAssembler.cpp's `dispOf()`). A Global's address is a
// fixed label loaded via `sethi`+`or`; a Local/Param's address instead varies per call,
// computed from `fp` with `add`/`sub` (see loadSlotAddr()).
//
// A narrower-than-int scalar loads/stores through the sign-/zero-extending byte/halfword
// mnemonics `ldindb`/`ldindsb`/`ldindh`/`ldindsh`/`stindb`/`stindh`; a `long long`
// loads/stores as two word accesses (low word at the base, high word at `base+4`).
//
// `*` uses hardware `smul` for a plain `int`-width multiply; a `long long` multiply uses
// the standard 32-on-32 cross-multiply decomposition (see genWideMul()), with the high
// half of each 32x32->64 product read back via `rdy`.
//
// `/` uses hardware `sdiv`/`udiv`, which computes a 64-bit `Y:rs1` dividend: the signed
// form needs Y seeded with the dividend's sign bits first (`sra rs1,31,t2` then
// `wry t2,g0`), the unsigned form needs Y seeded with 0. There is no remainder
// instruction, so `%` is synthesized as divide, multiply back out, subtract. A `long
// long` divide/modulo falls back to bit-serial shift/compare/subtract long division (see
// genWideDivMod()).
//
// `<<`/`>>` use hardware `sll`/`srl`/`sra` (`>>` is `srl` unless the type is signed). A
// `long long` shift falls back to a counted loop of 1-bit shifts carrying the crossing
// bit by hand (see genWideShift()).
//
// SPARC has no compare-and-set instruction: a comparison needs `cmp` (`subcc rs1,rs2,%g0`)
// plus a conditional branch. `isUnsigned` picks the plain or unsigned branch family for
// ordered comparisons (see genCompare()). A `long long` comparison compares high words
// first, falling back to an unsigned low-word compare only when they're equal (see
// genWideCompare()).
//
// Every SPARC branch and `call` has a one-instruction delay slot, always filled with a
// `nop` synthesized by SparcAssembler.cpp rather than emitted explicitly here.
class SparcCodegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "sparc"; }
    TypeWidths typeWidths() const override { return {1, 2, 4, 4, 8}; }
    std::string sourceExtension() const override { return ".ssparc"; }
    std::vector<std::string> assembledFormats() const override {
        return {"raw", "coff", "elf", "macho", "aout", "objraw"};
    }
    // `raw` is this backend's whole-program flat binary; the other five mean a real
    // relocatable object file for separate compilation.
    bool isObjectFormat(const std::string &format) const override { return format != "raw" && format != "asm"; }
    std::map<std::string, std::string> perFileAsm() const override;
    std::optional<std::string> assembleInProcess(const std::string &asmText,
                                                  const std::string &format) const override;

   private:
    void genFunction(const ir::FunctionBody &fn);
    void genStmt(const ir::Stmt &s);
    void genExpr(const ir::Expr &e);  // leaves the result in t0 (or t1:t0 for a long long)

    void genBinary(const ir::Expr &e);
    void genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalNot(const ir::Expr &rhs);
    void genMul(const ir::Expr &lhs, const ir::Expr &rhs);
    void genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned);
    void genIndex(const ir::Expr &e);         // slot[idx] read
    void genAddrOf(const ir::Expr &operand);  // &slot or &slot[idx]
    void genLocalArrayInit(const ir::Slot *slot);  // stores arrayInit into a Local's frame slots, at runtime
    // CompoundAssign/CompoundDerefAssign's "new = old <op> rhs" step - `old` in t0, `rhs`
    // in t1 (both at `type`'s width/signedness), leaves the raw new value in t0. `type`
    // is never wide here. The "/"/"%" case uses t2/t5 as scratch; callers that still need
    // t2 afterward must preserve it across this call.
    void genCompoundOp(const std::string &op, Type type);
    // Shared tail for CompoundAssign's indexed form and CompoundDerefAssign - t2 must
    // already hold the resolved address on entry, and is stashed on the stack across
    // `e.rhs`'s evaluation and genCompoundOp()'s own call, since either can clobber it.
    void genCompoundThroughT2(const ir::Expr &e);

    int widthOfType(Type t) const { return pcc::widthOf(t, typeWidths()); }
    bool isWide(Type t) const { return widthOfType(t) > widthOfType(Type{BaseType::Int, false}); }
    // log2 of an element type's width (1/2/4/8 bytes -> 0/1/2/3), the shift amount
    // genIndexAddr() scales an index by.
    int elemShift(Type t) const {
        switch (widthOfType(t)) {
            case 1: return 0;
            case 2: return 1;
            case 4: return 2;
            default: return 3;
        }
    }
    // Loads a value of `type` from the address in `addrReg` into `t0` (or `t1:t0` for a
    // `long long`), sign-/zero-extending a narrower-than-int source per its signedness.
    void loadOperand(const std::string &addrReg, Type type);
    // Stores `t0` (or `t1:t0`) into the address in `addrReg`, truncating to `type`'s width.
    void storeOperand(const std::string &addrReg, Type type);
    // Loads/stores a value of `type` at `fp+disp` directly (the Local/Param fast path),
    // using this dialect's displacement-operand `ldind`/`stind` form.
    void loadOperandDisp(int disp, Type type);
    void storeOperandDisp(int disp, Type type);
    // Re-extends `t0` from its low widthOf(to) bytes, per `to`'s signedness.
    void narrowT0To(Type to);
    void genConvert(const ir::Expr &e);
    void genWideAdd(const ir::Expr &lhs, const ir::Expr &rhs, bool isAdd);
    void genWideMul(const ir::Expr &lhs, const ir::Expr &rhs);
    // Software 64-bit divide/modulo via bit-serial shift/compare/subtract long division.
    // Leaves the result in t1:t0.
    void genWideDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned);
    void genWideShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned);
    void genWideCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned);
    // Negates the 64-bit value already in t1:t0 (hi:lo), in place.
    void genNegate64();

    // Leaves slot[idxExpr]'s effective address in `t2`.
    void genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr);

    // Loads `lbl`'s address into register `reg` (sethi+or). Only called with a Global's
    // or Function's name-derived label - a Local/Param's address goes through
    // loadSlotAddr() below instead.
    void loadAddress(const std::string &reg, const std::string &lbl);
    // `or g0,src,dst` - SPARC's synthetic "move register" idiom.
    void movReg(const std::string &dst, const std::string &src);

    // Leaves `slot`'s effective address in register `reg` - a Global goes through
    // loadAddress()/label(); a Local or Param goes through `fp`: `fp+frameOffset` for a
    // Param, `fp-frameOffset` for a Local (`frameOffset` is already a byte offset - see
    // IR.h's Slot::frameOffset). Emits a plain `or g0,fp,reg` when the offset is 0.
    void loadSlotAddr(const std::string &reg, const ir::Slot *slot);
    // The byte offset from `fp` loadSlotAddr() computes for a Local or Param (never
    // called for a Global). A Local array's is its element 0, its lowest address (see
    // ir::localLowOffset()).
    int slotFrameByteOffset(const ir::Slot *slot) const {
        if (slot->kind == ir::SlotKind::Param) return slot->frameOffset;
        return -ir::localOffsetDescendingWide(*slot, typeWidths());
    }

    // Loads/stores `slot`'s scalar value from/into `t0` (or `t1:t0`) - a Global goes
    // through loadAddress()+loadOperand()/storeOperand(); a Local or Param goes directly
    // through loadOperandDisp()/storeOperandDisp() at `slotFrameByteOffset()`.
    void loadSlotValue(const ir::Slot *slot);
    void storeSlotValue(const ir::Slot *slot);

    // Loads a plain 32-bit literal into register `reg`, picking a small immediate or a
    // sethi+or pair since no single SPARC instruction carries a full 32-bit immediate.
    void loadImmediate(const std::string &reg, int32_t v);
    // Restores `lr`/`fp`/`sp` to the calling function's values, right before a `retl`.
    // Shared by the fall-through epilogue and an early-exit Return; never touches
    // `t0`/`t1`, which already hold the return value by then.
    void emitEpilogue();

    // Only called for a Global slot - a Local/Param's address is computed fresh per call
    // through loadSlotAddr() instead.
    static std::string label(const ir::Slot *slot) { return "g_" + slot->sourceName; }
    static std::string label(const ir::Function *fn) { return "fn_" + fn->sourceName; }

    void declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values, Type elemType);
    std::string arrayElemText(const ir::ArrayInitElem &e);
    std::string newLabel() { return "L" + std::to_string(labelCounter_++); }
    std::string irLabel(int id) {
        auto it = irLabels_.find(id);
        if (it != irLabels_.end()) return it->second;
        std::string name = newLabel();
        irLabels_[id] = name;
        return name;
    }

    std::ostringstream code_;
    std::vector<std::string> trailer_;  // "label: dw v0, v1, ..." data lines, emitted at the end
    std::map<std::string, std::string> perFileAsm_;

    int labelCounter_ = 0;
    // One entry per open loop (innermost last) - {continue label, break label}.
    std::vector<std::pair<std::string, std::string>> loopLabels_;
    std::map<int, std::string> irLabels_;  // see irLabel()
};

}  // namespace pcc
