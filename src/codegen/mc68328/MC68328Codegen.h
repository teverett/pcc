#pragma once

#include <algorithm>
#include <map>
#include <optional>
#include <sstream>
#include <utility>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace rcc {

// Generates 68000 assembly for the Motorola/Freescale MC68328 "DragonBall" SoC (used in the
// original Palm Pilot). Its CPU core is a real MC68EC000, bit-for-bit the same instruction set as
// a plain MC68000 (no MMU, no extra/missing opcodes); its on-chip peripherals (UART, LCD
// controller, RTC, PLL, chip-select unit) are never touched by a bare no-OS compile.
//
// Unlike ../m68k/M68kCodegen.cpp (which relies on its simulator setting PC directly, needing no
// reset vector table), this backend emits a real one: chip-select A is based at address 0 at
// reset so the CPU's reset exception can fetch initial SSP from vector 0 and initial PC from
// vector 1. `_start` still reloads A7 explicitly too, since some simulators (confirmed on
// qemu-system-m68k's `-M none`) honor the PC half of reset but not the A7 half.
class MC68328Codegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "mc68328"; }
    TypeWidths typeWidths() const override { return {1, 2, 4, 4, 8}; }
    std::string sourceExtension() const override { return ".s328"; }
    std::vector<std::string> assembledFormats() const override {
        return {"raw", "coff", "elf", "macho", "aout", "objraw"};
    }
    // `raw` is a whole-program flat image (reset vector table plus code+data); the other five
    // formats are relocatable object files for separate compilation.
    bool isObjectFormat(const std::string &format) const override { return format != "raw" && format != "asm"; }
    std::map<std::string, std::string> perFileAsm() const override;
    std::optional<std::string> assembleInProcess(const std::string &asmText,
                                                  const std::string &format) const override;

   private:
    void genFunction(const ir::FunctionBody &fn);
    void genStmt(const ir::Stmt &s);
    void genExpr(const ir::Expr &e);  // leaves the result in D0

    void genBinary(const ir::Expr &e);
    void genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs);
    void emitCompare(const ir::Expr &lhs, const ir::Expr &rhs, const std::string &branchIfTrue);
    void genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalNot(const ir::Expr &rhs);
    void genMul(const ir::Expr &lhs, const ir::Expr &rhs);
    void genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned);
    void genIndex(const ir::Expr &e);         // slot[idx] read
    void genAddrOf(const ir::Expr &operand);  // &slot or &slot[idx]
    void genLocalArrayInit(const ir::Slot *slot);  // stores arrayInit into a Local's own frame slots
    void genConvert(const ir::Expr &e);
    // CompoundAssign/CompoundDerefAssign's "new = old <op> rhs" step: old in D0, rhs in D1,
    // leaves the raw (not yet narrowed) new value in D0. `type` is never wide here.
    void genCompoundOp(const std::string &op, Type type);
    // Shared tail for CompoundAssign's indexed form and CompoundDerefAssign; A0 must already
    // hold the resolved address and is preserved across `e.rhs`'s evaluation via the stack.
    void genCompoundThroughA0(const ir::Expr &e);

    // A `long long` result lives in D0(lo):D1(hi).
    void genWideAdd(const ir::Expr &lhs, const ir::Expr &rhs, bool isAdd);
    // 68000 has no 32x32 hardware multiply; a `long long` multiply cross-multiplies its 32-bit
    // halves via genMul()'s 16-bit-limited primitive rather than a true schoolbook multiply.
    void genWideMul(const ir::Expr &lhs, const ir::Expr &rhs);
    // Software 64-bit divide/modulo (68000's DIVS.W divisor is only 16 bits): bit-serial
    // shift/compare/subtract long division addressed via a0 over a stack scratch block. Leaves
    // the result in D0(lo):D1(hi).
    void genWideDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned);
    void genWideShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned);
    void genWideCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned);
    // Negates the 64-bit value already in D0(lo):D1(hi), in place.
    void genNegate64();
    // 32-bit division: d1 = dividend, d0 = divisor on entry; d3 =
    // quotient, d2 = remainder on exit. Clobbers d0-d7; preserves a0.
    void emitDivMod32(bool isUnsigned);

    // Leaves slot[idxExpr]'s own effective address in `A0`.
    void genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr);

    int widthOfType(Type t) const { return rcc::widthOf(t, typeWidths()); }
    bool isWide(Type t) const { return widthOfType(t) > widthOfType(Type{BaseType::Int, false}); }
    // Number of "addl d0,d0" doublings to scale an index by this element type's width
    // (1/2/4/8 bytes -> 0/1/2/3 doublings).
    int elemDoublings(Type t) const {
        switch (widthOfType(t)) {
            case 1: return 0;
            case 2: return 1;
            case 4: return 2;
            default: return 3;
        }
    }

    // Loads a value of `type` from operand text `mem` into D0 (or D0:D1 for a `long long`),
    // sign- or zero-extending a narrower-than-int source per its signedness. `highMem` is the
    // high dword's operand text for the wide case.
    void loadOperand(const std::string &mem, const std::string &highMem, Type type);
    void storeOperand(const std::string &mem, const std::string &highMem, Type type);
    // Re-extends D0 from its low widthOf(to) bytes per `to`'s signedness, for a Convert between
    // two types no wider than a native int.
    void narrowD0To(Type to);
    // Indirect-addressing counterpart of loadOperand()/storeOperand(), through the address
    // already in A0. The wide case bumps A0 by 4 to reach the high word.
    void loadIndirect(Type type);
    void storeIndirect(Type type);

    // Global slots and Functions get a name-based label; a Local/Param is addressed relative to
    // `a6` instead (see addr()), since a Local is never visible outside its own function.
    static std::string label(const ir::Slot *slot) { return "g_" + slot->sourceName; }
    static std::string label(const ir::Function *fn) { return "fn_" + fn->sourceName; }

    // A Local or Param's frame-relative operand text, e.g. "[a6-8]"/"[a6+12]", optionally offset
    // by `byteOffset` (e.g. a `long long`'s high dword, 4 bytes further in). `linkl a6,-N` (see
    // genFunction()) pushes the caller's a6, points this call's a6 at it, and reserves N bytes
    // of locals below it: a Param grows upward past the saved a6/return address, a Local grows
    // downward into the reserved locals. `Slot::frameOffset` already accounts for width/alignment.
    // A Local array is addressed from its lowest address, element 0 (see ir::localLowOffset()),
    // and `byteOffset` counts upward from there, the same direction as a Global array's elements.
    std::string addr(const ir::Slot *slot, int byteOffset = 0) const {
        if (slot->kind == ir::SlotKind::Param) {
            return "[a6+" + std::to_string(slot->frameOffset + paramBigEndianFixup(slot) + byteOffset) + "]";
        }
        if (slot->arraySize > 0) {
            return "[a6-" + std::to_string(ir::localLowOffset(*slot, typeWidths()) - byteOffset) + "]";
        }
        return "[a6-" + std::to_string(ir::localOffsetDescendingWide(*slot, typeWidths()) + byteOffset) + "]";
    }

    // Operand text for a plain read/write of `slot`, `byteOffset` bytes in: a Global uses its
    // bare label, a Local/Param uses addr().
    std::string operandOf(const ir::Slot *slot, int byteOffset = 0) const {
        if (slot->kind != ir::SlotKind::Global) return addr(slot, byteOffset);
        std::string lbl = label(slot);
        return byteOffset == 0 ? lbl : (lbl + "+" + std::to_string(byteOffset));
    }

    // On this big-endian target, a param narrower than the reserved native-int slot has its real
    // bytes at the HIGH end of that slot, not the start; this gives the byte offset to that end.
    // Zero for long/long long, which already fill or exceed the reservation.
    int paramBigEndianFixup(const ir::Slot *slot) const {
        int reserved = std::max(widthOfType(slot->type), widthOfType(Type{BaseType::Int, false}));
        return reserved - widthOfType(slot->type);
    }

    std::string allocTemp() { return "t" + std::to_string(tempCounter_++); }
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
    std::vector<std::string> trailer_;  // "label: dl v0, v1, ..." data lines, emitted once at the end
    std::map<std::string, std::string> perFileAsm_;

    int tempCounter_ = 0;
    int labelCounter_ = 0;
    // One entry per currently-open loop (innermost last): {continue label, break label}.
    std::vector<std::pair<std::string, std::string>> loopLabels_;
    std::map<int, std::string> irLabels_;  // see irLabel()
};

}  // namespace rcc
