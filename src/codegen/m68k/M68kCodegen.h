#pragma once

#include <map>
#include <optional>
#include <sstream>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace pcc {

// Generates 68000 assembly (this project's own dialect - see
// M68kAssembler.h) for the IR built by IRBuilder. See
// ../../../README.md for the shared language and this backend's own
// "bare flat binary" platform choice: `generate()` returns a
// complete, self-contained program loaded at a fixed address
// (`kBaseAddress`, see M68kCodegen.cpp) - no OS, no boot ROM, no
// vector-table-driven reset (the simulator sets the CPU's PC to
// `kBaseAddress` directly - see tests/run_tests.sh's own qemu
// invocation - rather than this backend ever having to lay out a
// real reset vector itself), halting in an infinite loop, the same
// "no stack, no recursion" model every backend shares (see README's
// "Scope and limitations") - just on a CPU rich enough that almost
// everything maps onto a real instruction directly instead of
// needing a workaround.
//
// This language's own `int` is 32 bits here (`.L` throughout) - this
// target's native register/address width, the same reasoning every
// other backend's own word-size choice follows (pdp7's 18 bits,
// 8086/6502's 16 bits - see I8086Codegen.h/M6502Codegen.h's own
// comments). Every value in flight lives in one of two data
// registers: `D0` (genExpr()'s own result - the "ac" every other
// backend also calls it) and `D1` ("the other operand" in a binary
// op - 8086's "bx"). Address register `A0` holds a computed pointer
// when dereferencing one (array/pointer indexing, `*`/`&`) - the
// 68000, unlike the 6502 (see M6502Codegen.h's own header comment on
// why it needs a dedicated zero-page register for this), has real
// address registers and a real `(An)` indirect addressing mode with
// no special placement requirement at all. `A7` is the real hardware
// stack pointer - saving one operand across evaluating the other in
// a binary expression, and every function call/return, use real
// PUSH/POP-equivalent moves and real `JSR`/`RTS`, no synthesis
// needed (unlike pdp7 - see Pdp7Codegen.cpp's own comment on why it
// has to improvise this with named memory cells instead).
//
// `*`/`/` use the real hardware `MULS.W`/`DIVS.W` - the *original*
// 68000's only multiply/divide, word-sized even though this
// backend's own int is long: `MULS.W` takes two 16-bit operands and
// produces a correct 32-bit product (so it's only wrong if the true
// mathematical product doesn't fit in 32 bits - the same kind of
// edge case other backends already accept, e.g. this project's own
// 16-bit `int` overflowing on pdp7/8086/6502), but `DIVS.W` divides a
// 32-bit dividend by a 16-bit divisor - genuinely wrong once the
// divisor itself needs more than 16 bits, not just when the final
// answer would be out of range. A real 32-bit multiply/divide would
// need a software routine built from these (splitting each operand
// into halves) the way pdp7/6502's own multiply/divide already are
// software routines for a different reason (no hardware multiply/
// divide at all) - not implemented here, since every value in this
// project's own test programs comfortably fits in 16 bits.
//
// `<<`/`>>` use the real hardware variable-count register-form shift
// (`LSL.L`/`LSR.L Dcount,Ddata`) - no software loop needed, unlike
// pdp7/6502 (neither has a variable-count shift instruction at all -
// see their own comments). `>>` is `LSR` (logical/unsigned), the same
// convention (and the same reason - no unsigned type to make the
// distinction with) every other backend's own `>>` already uses.
//
// Comparisons use real signed `CMP.L` + `Bcc` directly - the 68000's
// own condition codes already encode signed less-than/greater-than
// correctly in hardware (same as 8086's `cmp`+`jl`/`jle`/etc. - see
// I8086Codegen.h), unlike the 6502, which has no combined signed-
// compare flag and needs an explicit `N xor V` fixup (see
// M6502Codegen.h's own comment) - nothing like that is needed here.
//
// Every `Bcc`/`BRA` here always uses the 16-bit-displacement
// instruction form (see M68kAssembler.h's own comment) - a real
// ±32KB range, more than this project's own test programs could ever
// need, so (unlike the 6502's ±127-byte branches - see
// M6502Codegen.h's own `safeBranch()`) there's no branch-range
// workaround needed anywhere in this backend.
class M68kCodegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "68000"; }
    std::string sourceExtension() const override { return ".s68"; }
    std::vector<std::string> assembledFormats() const override { return {"raw"}; }
    std::string defaultAssemblerPath() const override { return ""; }
    std::string buildAssembleCommand(const std::string &assemblerPath, const std::string &format,
                                      const std::string &srcPath, const std::string &outPath) const override;
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
    void genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder);
    void genIndex(const ir::Expr &e);         // slot[idx] read
    void genAddrOf(const ir::Expr &operand);  // &slot or &slot[idx]

    // Leaves slot[idxExpr]'s own effective address in `A0`.
    void genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr);

    static std::string label(const ir::Slot *slot) {
        if (slot->kind == ir::SlotKind::Global) return "g_" + slot->sourceName;
        if (slot->kind == ir::SlotKind::Param) {
            return "p_" + slot->ownerFunctionName + "_" + std::to_string(slot->paramIndex);
        }
        return slot->sourceName + "_" + std::to_string(slot->id);
    }
    static std::string label(const ir::Function *fn) { return "fn_" + fn->sourceName; }

    std::string allocTemp() { return "t" + std::to_string(tempCounter_++); }
    void declareTemp(const std::string &lbl);
    void declareArray(const std::string &lbl, const std::vector<long> &values);
    std::string newLabel() { return "L" + std::to_string(labelCounter_++); }

    std::ostringstream code_;
    std::vector<std::string> trailer_;  // "label: dl v0, v1, ..." data lines, emitted once at the end
    std::map<std::string, std::string> perFileAsm_;

    int tempCounter_ = 0;
    int labelCounter_ = 0;
};

}  // namespace pcc
