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

// Generates MIPS assembly (see MipsAssembler.h) for the IR built by
// IRBuilder, targeting MIPS I (runs unchanged on R10000/R4000/etc, so
// `--target=r4000` is a software-identical alias of `r10000`).
// `generate()` emits a self-contained program loaded at a fixed
// address (`kBaseAddress`), halting in a self-branch loop after
// `main` returns. `int` is 32 bits (only the low 32 bits of any GPR
// are ever meaningful).
//
// Register convention:
//   $t0 - genExpr()'s result register ("ac")
//   $t1 - the other operand in a binary op
//   $t2 - holds a computed address when dereferencing one
//   $at - scratch for a comparison's SLT, and for the caller's saved
//         $fp while the epilogue restores it
//   $sp - hardware stack pointer: used both to save an operand across
//         evaluating the other in a binary expression, and for calls
//   $ra - hardware link register JAL sets and JR reads
//   $fp - this call's frame pointer (o32 ABI's $s8); every prologue
//         points it at its own saved-fp word, giving each call
//         (including recursive ones) independent Local/Param storage
//
// Frame layout relative to $fp: fp+0 is the caller's saved $fp, fp+4
// the saved $ra, param i at fp+8+4*i (right-to-left push, caller
// cleanup); locals are below fp, reserved by the prologue's
// `ADDIU sp,sp,-N`, with Slot::frameOffset K at fp-4*K.
//
// MIPS has no absolute-addressing memory operand: every load/store
// goes through a base register plus a 16-bit displacement, so
// accessing any slot first loads its address into $t2 via LUI+ORI,
// then uses `ldind`/`stind $t2,$t0` (this dialect's LW/SW with a
// fixed zero displacement).
//
// `*`/`/` use the real hardware MULT/DIV (MFLO/MFHI pull the 32-bit
// result back out of HI/LO), full 32-bit operands, no width limit.
// `<<`/`>>` use the register-form shift SLLV/SRLV (`>>` is logical,
// SRLV not SRAV); a fixed index*4 scale uses SLL instead.
// Comparisons use SLT/SLTU/SLTIU directly as the 1/0 boolean result,
// no branch needed.
//
// J/JAL/JR/BEQ/BNE all have a real hardware branch-delay slot (the
// following instruction always executes before the transfer takes
// effect); this backend always emits a NOP right after, unfilled.
class MipsCodegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "r10000"; }
    TypeWidths typeWidths() const override { return {1, 2, 4, 4, 8}; }
    std::string sourceExtension() const override { return ".smips"; }
    std::vector<std::string> assembledFormats() const override {
        return {"raw", "coff", "elf", "macho", "aout", "objraw"};
    }
    // `raw` is a whole-program flat binary; the other five are real
    // relocatable object files for separate compilation.
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
    void genExpr(const ir::Expr &e);  // leaves the result in $t0

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
    // CompoundAssign/CompoundDerefAssign's "new = old <op> rhs" step:
    // old in t0, rhs in t1, leaves the untruncated new value in t0.
    void genCompoundOp(const std::string &op, Type type);
    // Shared tail for CompoundAssign/CompoundDerefAssign; t2 must
    // already hold the resolved target address on entry.
    void genCompoundThroughT2(const ir::Expr &e);

    int widthOfType(Type t) const { return pcc::widthOf(t, typeWidths()); }
    bool isWide(Type t) const { return widthOfType(t) > widthOfType(Type{BaseType::Int, false}); }
    // log2 of an element type's width (1/2/4/8 bytes -> 0/1/2/3).
    int elemShift(Type t) const {
        switch (widthOfType(t)) {
            case 1: return 0;
            case 2: return 1;
            case 4: return 2;
            default: return 3;
        }
    }
    // Loads a value of `type` from `addrReg` into t0 (or t1:t0 for a
    // `long long`), sign- or zero-extending per its signedness.
    void loadOperand(const std::string &addrReg, Type type);
    // Stores t0 (or t1:t0) into `addrReg`, truncating to `type`'s width.
    void storeOperand(const std::string &addrReg, Type type);
    // Re-extends t0 from its low widthOf(to) bytes, per `to`'s signedness.
    void narrowT0To(Type to);
    void genConvert(const ir::Expr &e);
    void genWideAdd(const ir::Expr &lhs, const ir::Expr &rhs, bool isAdd);
    void genWideMul(const ir::Expr &lhs, const ir::Expr &rhs);
    // Software 64-bit divide/modulo (no 32-bit MIPS instruction is wide
    // enough): bit-serial shift/compare/subtract long division. Leaves
    // the result in t1:t0.
    void genWideDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned);
    void genWideShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned);
    void genWideCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned);
    // Negates the 64-bit value already in t1:t0 (hi:lo), in place.
    void genNegate64();

    // Leaves slot[idxExpr]'s effective address in $t2.
    void genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr);

    void loadAddress(const std::string &reg, const std::string &lbl);

    // Loads a plain (non-array) Global/Local/Param slot's address into
    // `reg`: a Global's fixed label address, or a Local/Param's
    // $fp-relative address computed in one ADDIU.
    void loadSlotAddr(const std::string &reg, const ir::Slot *slot);

    // Emits a 32-bit literal load into `reg`: ADDIU when it fits in 16
    // bits (sign-extended), LUI+ORI otherwise.
    void loadImm32(const std::string &reg, int32_t v);

    // Shared function-exit sequence (deallocate locals, restore
    // caller's $fp/$ra, JR ra); used by both the fall-through epilogue
    // and every explicit early `return`.
    void genEpilogue();

    // A Global's name-based label, or a Function's entry label. A
    // Local/Param never gets one; it's addressed via loadSlotAddr().
    static std::string label(const ir::Slot *slot) { return "g_" + slot->sourceName; }
    static std::string label(const ir::Function *fn) { return "fn_" + fn->sourceName; }

    void declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values, Type elemType);
    std::string arrayElemText(const ir::ArrayInitElem &e);
    std::string newLabel() { return "L" + std::to_string(labelCounter_++); }

    std::ostringstream code_;
    std::vector<std::string> trailer_;  // "label: dw v0, v1, ..." data lines, emitted once at the end
    std::map<std::string, std::string> perFileAsm_;

    int labelCounter_ = 0;
    // One entry per currently-open loop (innermost last): {continue
    // label, break label}.
    std::vector<std::pair<std::string, std::string>> loopLabels_;
};

}  // namespace pcc
