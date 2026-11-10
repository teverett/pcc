#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <sstream>
#include <utility>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace rcc {

// Generates PowerPC assembly (this project's own dialect - see
// PowerPCAssembler.h) for 32-bit classic PowerPC. `generate()` returns a
// self-contained program loaded at a fixed address (`kBaseAddress`), no
// OS/bootloader, halting in a self-branch infinite loop.
//
// Register assignment (this backend's own convention; there is no
// cross-function argument-passing convention - every parameter is a
// statically allocated slot - see genFunction()):
//   r3 ("t0") - "ac", genExpr()'s own result; also the real PowerPC ABI's
//               own return-value register, though otherwise unused here.
//   r4 ("t1") - the other operand in a binary op
//   r5 ("t2") - holds a computed address when dereferencing one
//   r1 ("sp") - the real hardware stack pointer, used both to save one
//               operand across evaluating the other and for every call
//   r0        - scratch, used only to shuttle the link register through
//               `mflr`/`mtlr` around a call (PowerPC's link register, unlike
//               MIPS's `$ra`, isn't directly memory-addressable)
//   r31 ("fp") - this backend's own frame pointer: set once per call in
//               genFunction()'s prologue and held fixed for that
//               activation even while `sp` moves transiently; every
//               Local/Param addresses relative to `fp` (see slotDisp()),
//               which is what makes recursion safe.
//
// PowerPC has no absolute-addressing memory operand (like MIPS): every
// load/store goes through a base register plus displacement, so accessing
// any global/local/array slot first loads its address into `t2` via
// `lis`+`ori`, then `ldind`/`stind`.
//
// `*`/`/` use the real hardware `mullw`/`divw`. `%` has no direct hardware
// remainder instruction, so it's synthesized: `divw` for the quotient,
// `mullw` to multiply back out, `subf` to recover the remainder (see
// genDivMod()).
//
// `<<`/`>>` use the real hardware variable-count shift (`slw`/`srw`); `>>`
// is always logical (`srw`), never arithmetic (`sraw`).
//
// Comparisons use `cmpw` plus a conditional branch to materialize the 0/1
// boolean, since PowerPC (unlike MIPS's `slt`) has no compare-and-set
// instruction (see genCompare()).
//
// PowerPC branches/calls have no branch-delay slot.
class PowerPCCodegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "powerpc"; }
    TypeWidths typeWidths() const override { return {1, 2, 4, 4, 8}; }
    std::string sourceExtension() const override { return ".sppc"; }
    std::vector<std::string> assembledFormats() const override {
        return {"raw", "coff", "elf", "macho", "aout", "objraw"};
    }
    // `raw` is this backend's own whole-program flat binary; the other five
    // mean a real relocatable object file for separate compilation.
    bool isObjectFormat(const std::string &format) const override { return format != "raw" && format != "asm"; }
    std::map<std::string, std::string> perFileAsm() const override;
    std::optional<std::string> assembleInProcess(const std::string &asmText,
                                                  const std::string &format) const override;

   private:
    void genFunction(const ir::FunctionBody &fn);
    // Restores `fp`/the link register from the words genFunction()'s
    // prologue saved them to, and leaves `sp` back where it was on entry.
    // Shared by the fall-through return and every early `return`. Never
    // emits `blr` itself - the caller emits that right after.
    void emitEpilogue();
    void genStmt(const ir::Stmt &s);
    void genExpr(const ir::Expr &e);  // leaves the result in t0

    void genBinary(const ir::Expr &e);
    void genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalNot(const ir::Expr &rhs);
    void genMul(const ir::Expr &lhs, const ir::Expr &rhs);
    void genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned);
    void genIndex(const ir::Expr &e);         // slot[idx] read
    void genAddrOf(const ir::Expr &operand);  // &slot or &slot[idx]
    void genLocalArrayInit(const ir::Slot *slot);  // stores arrayInit into a Local's own frame slots
    // CompoundAssign/CompoundDerefAssign's "new = old <op> rhs" step: `old`
    // already in t0, `rhs` already in t1, leaves the raw new value in t0.
    // `type` is never wide here (IRBuilder.cpp guarantees it).
    void genCompoundOp(const std::string &op, Type type);
    // Shared tail for indexed CompoundAssign/CompoundDerefAssign; t2 must
    // already hold the resolved address on entry and is preserved across
    // evaluating `e.rhs` via the real hardware stack (PowerPC has no
    // dedicated push/pop).
    void genCompoundThroughT2(const ir::Expr &e);

    int widthOfType(Type t) const { return rcc::widthOf(t, typeWidths()); }
    bool isWide(Type t) const { return widthOfType(t) > widthOfType(Type{BaseType::Int, false}); }
    int elemShift(Type t) const {
        switch (widthOfType(t)) {
            case 1: return 0;
            case 2: return 1;
            case 4: return 2;
            default: return 3;
        }
    }
    // Loads a value of `type` from the address in `addrReg` into `reg` (or
    // `t1:reg`, high:low, for a `long long`), sign/zero-extending a
    // narrower-than-native-int source.
    void loadOperand(const std::string &addrReg, const std::string &reg, Type type);
    // Stores `reg` (or `t1:reg`) into the address in `addrReg`, truncating
    // to `type`'s width.
    void storeOperand(const std::string &addrReg, const std::string &reg, Type type);
    // Loads a value of `type` from `fp+disp` directly (the Local/Param fast
    // path), without a separate address computation.
    void loadOperandDisp(int disp, const std::string &reg, Type type);
    void storeOperandDisp(int disp, const std::string &reg, Type type);
    void narrowT0To(Type to);
    void genConvert(const ir::Expr &e);
    void genWideAdd(const ir::Expr &lhs, const ir::Expr &rhs, bool isAdd);
    void genWideMul(const ir::Expr &lhs, const ir::Expr &rhs);
    // Software 64-bit divide/modulo (no 32-bit PowerPC instruction is wide
    // enough); same bit-serial algorithm as I386Codegen.cpp's/
    // MipsCodegen.cpp's genWideDivMod(). Leaves the result in t1:t0.
    void genWideDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned);
    void genWideShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned);
    void genWideCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned);
    // Negates the 64-bit value already in t1:t0 (hi:lo), in place.
    void genNegate64();

    // Leaves slot[idxExpr]'s own effective address in `t2`.
    void genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr);

    // Loads `lbl`'s address into register `reg` (lis+ori).
    void loadAddress(const std::string &reg, const std::string &lbl);

    // Loads a compile-time-known 32-bit literal into `reg` - `li` when it
    // fits 16 bits signed, `lis`+`ori` otherwise.
    void loadImmediate(const std::string &reg, int32_t v);

    // A Global loads/stores through its address label; a Local or Param
    // goes straight through `ldindd`/`stindd fp,reg,slotDisp(slot)`. Never
    // called with `reg` equal to "t2" (the Global path's own scratch
    // register).
    void loadSlotValue(const std::string &reg, const ir::Slot *slot);
    void storeSlotValue(const ir::Slot *slot, const std::string &reg);

    // Only for a Global: a real separate-compilation label shared by every
    // file referencing this symbol by name.
    static std::string label(const ir::Slot *slot) { return "g_" + slot->sourceName; }
    static std::string label(const ir::Function *fn) { return "fn_" + fn->sourceName; }

    // A Local or Param's signed byte displacement from `fp`. genFunction()'s
    // prologue leaves `fp` pointing at the saved-old-`fp` word, with the
    // return address above it and the caller's pushed arguments (param 0
    // closest) above that - so a Param's displacement is `+frameOffset`, and
    // a Local's is its lowest address (see ir::localLowOffset()), with an
    // array's element i or a wide value's later words above it.
    int slotDisp(const ir::Slot *slot) const {
        if (slot->kind == ir::SlotKind::Param) return slot->frameOffset;
        return -ir::localLowOffset(*slot, typeWidths());
    }

    void declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values, Type elemType);
    std::string arrayElemText(const ir::ArrayInitElem &e);
    std::string newLabel() { return "L" + std::to_string(labelCounter_++); }
    // Maps an ir::Stmt::labelId (switch's own Label/Goto - see IR.h) to this
    // backend's own label text, generated the first time either a Label
    // defining it or a Goto referencing it is seen.
    std::string irLabel(int id) {
        auto it = irLabels_.find(id);
        if (it != irLabels_.end()) return it->second;
        std::string name = newLabel();
        irLabels_[id] = name;
        return name;
    }

    std::ostringstream code_;
    std::vector<std::string> trailer_;  // "label: dw v0, v1, ..." data lines, emitted once at the end
    std::map<std::string, std::string> perFileAsm_;

    int labelCounter_ = 0;
    // One entry per currently-open loop (innermost last): {continue label,
    // break label}.
    std::vector<std::pair<std::string, std::string>> loopLabels_;
    std::map<int, std::string> irLabels_;  // see irLabel()
};

}  // namespace rcc
