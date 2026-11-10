#pragma once

#include <algorithm>
#include <map>
#include <optional>
#include <sstream>
#include <utility>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace pcc {

// Generates this project's 68000 dialect (see M68kAssembler.h) for the IR built by
// IRBuilder, as a flat binary loaded at a fixed address (kBaseAddress) with no OS,
// boot ROM, or reset vector; the simulator sets the CPU's PC there directly.
//
// `int` is 32 bits (.L). D0 holds genExpr()'s result, D1 the other operand of a
// binary op, A0 a computed pointer for array/pointer dereference, A7 the real
// hardware stack pointer (used directly for push/pop and JSR/RTS).
//
// A6 is the frame pointer: LINK/UNLK give every call its own activation record, so
// locals/params are addressed A6-relative (see addr()) and recursion is safe.
//
// `*`/`/` use MULS.W/DIVS.W, the original 68000's only multiply/divide: MULS.W's
// 16x16->32 product is always correct, but DIVS.W's divisor is only 16 bits, so
// results are wrong once the divisor needs more than 16 bits. No software 32-bit
// multiply/divide is implemented; this project's test programs stay within 16 bits.
//
// `<<`/`>>` use the hardware variable-count shift (LSL.L/LSR.L); `>>` is logical (LSR).
//
// Comparisons use CMP.L + Bcc directly: the 68000's condition codes already encode
// signed comparisons correctly in hardware.
//
// Every Bcc/BRA uses the 16-bit-displacement form (+-32KB), ample for this project.
class M68kCodegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "68000"; }
    TypeWidths typeWidths() const override { return {1, 2, 4, 4, 8}; }
    std::string sourceExtension() const override { return ".s68"; }
    std::vector<std::string> assembledFormats() const override {
        return {"raw", "coff", "elf", "macho", "aout", "objraw"};
    }
    // `raw` is a whole-program flat binary; the others are relocatable object files.
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
    void genLocalArrayInit(const ir::Slot *slot);  // stores arrayInit into a Local's frame slots
    void genConvert(const ir::Expr &e);
    // new = old <op> rhs: old in D0, rhs in D1, leaves the un-narrowed result in D0
    // (see narrowD0To()). `type` is never wide here.
    void genCompoundOp(const std::string &op, Type type);
    // Shared tail for indexed CompoundAssign/CompoundDerefAssign; A0 must hold the
    // resolved address on entry and is preserved across evaluating e.rhs via the stack.
    void genCompoundThroughA0(const ir::Expr &e);

    // A `long long` result lives in D0(lo):D1(hi).
    void genWideAdd(const ir::Expr &lhs, const ir::Expr &rhs, bool isAdd);
    // 68000 has no 32x32 hardware multiply (only MULS.W, 16x16->32), so genMul() is
    // only correct when both operands fit in 16 bits. genWideMul() cross-multiplies
    // 32-bit halves via that same limited primitive rather than a true 4-limb multiply.
    void genWideMul(const ir::Expr &lhs, const ir::Expr &rhs);
    // Software 64-bit divide/modulo (DIVS.W's divisor is only 16 bits): bit-serial
    // shift/compare/subtract long division via a0 over a stack-resident scratch
    // block. Leaves the result in D0(lo):D1(hi).
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

    int widthOfType(Type t) const { return pcc::widthOf(t, typeWidths()); }
    bool isWide(Type t) const { return widthOfType(t) > widthOfType(Type{BaseType::Int, false}); }
    // Number of doubling ("addl d0,d0") steps to scale an index by this element width.
    int elemDoublings(Type t) const {
        switch (widthOfType(t)) {
            case 1: return 0;
            case 2: return 1;
            case 4: return 2;
            default: return 3;
        }
    }

    // Loads a value of `type` from frame/absolute operand text `mem` into D0 (or
    // D0:D1 for a `long long`), sign/zero-extending per signedness. `highMem` is the
    // high dword's operand text for the wide case.
    void loadOperand(const std::string &mem, const std::string &highMem, Type type);
    void storeOperand(const std::string &mem, const std::string &highMem, Type type);
    // Re-extends D0 from its low widthOf(to) bytes, per `to`'s signedness.
    void narrowD0To(Type to);
    // Loads/stores D0 (or D0:D1) through the address already in A0. The wide case
    // advances A0 by 4 to reach the high word.
    void loadIndirect(Type type);
    void storeIndirect(Type type);

    // Globals and Functions get a name-based label; a Local/Param is addressed
    // relative to `a6` instead (see addr()).
    static std::string label(const ir::Slot *slot) { return "g_" + slot->sourceName; }
    static std::string label(const ir::Function *fn) { return "fn_" + fn->sourceName; }

    // A Local/Param's frame-relative operand text, e.g. "[a6-8]"/"[a6+12]",
    // optionally offset by `byteOffset` more bytes (a `long long`'s high dword).
    // `linkl a6,-N` pushes the caller's a6, sets this call's a6 to it, then reserves
    // N bytes of locals below it. `Slot::frameOffset` already accounts for
    // width/alignment: a Param grows upward past the saved a6/return address, a
    // Local grows downward. A Local array is addressed from its lowest address,
    // element 0 (see ir::localLowOffset()), and `byteOffset` counts upward from
    // there, the same direction as a Global array's elements.
    std::string addr(const ir::Slot *slot, int byteOffset = 0) const {
        if (slot->kind == ir::SlotKind::Param) {
            return "[a6+" + std::to_string(slot->frameOffset + paramBigEndianFixup(slot) + byteOffset) + "]";
        }
        if (slot->arraySize > 0) {
            return "[a6-" + std::to_string(ir::localLowOffset(*slot, typeWidths()) - byteOffset) + "]";
        }
        return "[a6-" + std::to_string(ir::localOffsetDescendingWide(*slot, typeWidths()) + byteOffset) + "]";
    }

    // Operand text for `slot`, `byteOffset` bytes in: a Global uses its label, a
    // Local/Param uses addr().
    std::string operandOf(const ir::Slot *slot, int byteOffset = 0) const {
        if (slot->kind != ir::SlotKind::Global) return addr(slot, byteOffset);
        std::string lbl = label(slot);
        return byteOffset == 0 ? lbl : (lbl + "+" + std::to_string(byteOffset));
    }

    // On this big-endian target, a narrower-than-native-int param's real bytes sit
    // at the HIGH-address end of its reserved slot (e.g. a 4-byte push of 100 stores
    // 00 00 00 64), unlike little-endian i386. Nonzero only for char/short params.
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
    // One entry per open loop (innermost last): {continue label, break label}.
    std::vector<std::pair<std::string, std::string>> loopLabels_;
    std::map<int, std::string> irLabels_;  // see irLabel()
};

}  // namespace pcc
