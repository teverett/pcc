#pragma once

#include <map>
#include <optional>
#include <sstream>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace pcc {

// Generates Renesas M16C assembly (this project's own dialect - see
// M16cAssembler.h) for the IR built by IRBuilder. See
// ../../../README.md for the shared language and this backend's own
// "bare flat binary" platform choice: `generate()` returns a
// complete, self-contained program loaded at a fixed address
// (`kBaseAddress`, see M16cCodegen.cpp), halting in an infinite loop -
// no boot ROM, no on-chip peripheral setup - the same "no stack, no
// recursion" model every backend shares (see README's "Scope and
// limitations"), on a CPU with four real 16-bit general registers
// (`R0`-`R3`) plus two dedicated address registers (`A0`/`A1`) and a
// genuine hardware stack (`SP`, real `PUSH.W`/`POP.W`/`JSR.W`/`RTS`),
// so (like the pdp11/68000 backends) almost everything maps onto one
// real instruction, not a named memory cell the way the 8080/6502
// backends have to improvise (see I8080Codegen.h's own comment for
// why those targets need that instead).
//
// This language's own `int` is 16 bits here (the M16C's native word
// size) - every value in flight lives in one of two general registers:
// `R0` (genExpr()'s own result - the "ac" every other backend also
// calls it) and `R1` ("the other operand" in a binary op). `A0` holds
// a computed address when dereferencing one (array/pointer indexing,
// `*`/`&`) - a dedicated address register, not an ordinary general one
// the way the pdp11 backend's own `R2` is (see Pdp11Codegen.h), so it
// never collides with `R0`/`R1` at all and - unlike that backend's own
// `genIndexAddr()` - never needs saving across a nested subexpression
// that only ever touches `R0`/`R1`; it's only saved (via the real
// stack) across a nested subexpression that might *itself* compute
// another address into `A0` (see genStmt()'s own Assign case).
//
// `*`/`/` use real M16C hardware `MUL.W`/`DIVU.W` - `MUL.W src,R0`
// leaves a full 32-bit product in `R2:R0` (high word:low word, this
// project's own 16-bit truncated answer always being the low word,
// `R0`); `DIVU.W src,R0` divides the 32-bit dividend in `R2:R0` by a
// 16-bit divisor, quotient into `R0`, remainder into `R2`. Only the
// *unsigned* divide is used (`R2` is simply zeroed before dividing,
// never sign-extended) - the same "only verified against non-negative
// operands" scope limitation every other software-divide backend here
// already documents (see I8080Codegen.h's own comment), just because
// this target's signed `DIV.W` would need its own sign-extension
// sequence this backend doesn't bother synthesizing.
//
// `<<`/`>>` use the real variable-count `SHL.W count,dst` directly -
// unlike every loop-one-bit-at-a-time backend here (pdp7/6502/pdp11/
// 8080/8086 - see their own comments), M16C's own `SHL.W` takes a
// *signed* count operand in hardware: positive shifts left, negative
// shifts right, so `>>` is simply `SHL.W` with the count register
// negated first (`NEG.W`) - one real instruction either way, no loop.
// Still the usual logical (unsigned) convention for `>>` (no unsigned
// type exists to make the distinction with) - this backend's own
// `R2`-left-as-zero `DIVU.W` convention above aside, there's no sign-
// extension anywhere in a right shift for that same reason.
//
// Comparisons use real hardware `CMP.W` + a real signed conditional
// jump (`JLT`/`JLE`/`JGT`/`JGE`/`JEQ`/`JNE`) directly - the M16C's own
// condition codes already encode signed less-than/greater-than
// correctly in hardware (same reasoning as the pdp11/68000/8086
// backends - see M68kCodegen.h's own comment), unlike the 6502, which
// needs an explicit N-xor-V fixup. This dialect's own `CMP.W src,dst`
// sets the flags from `dst - src` (matching every other two-operand
// mnemonic's own `dst = dst OP src` convention - see
// M16cAssembler.h's own header comment), so `genCompare()`'s own
// `CMP.W R0,R1` (R1=lhs, R0=rhs) computes `lhs - rhs` directly, with
// no operand-order inversion needed anywhere else in this file.
//
// Every jump and conditional jump here is always encoded as a full,
// unconditional-range absolute address - a deliberate simplification
// of this dialect (real M16C conditional jumps are short, PC-relative,
// and need a separate long-range form once a target is too far away -
// see M16cAssembler.h's own header comment), the same simplicity
// tradeoff the 8080 backend's own header comment makes for the exact
// same reason: it means no branch-range workaround (the pdp11/6502
// backends' own `safeBranch()`) is needed anywhere in this file.
class M16cCodegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "m16c"; }
    std::string sourceExtension() const override { return ".s16c"; }
    std::vector<std::string> assembledFormats() const override { return {"bin"}; }
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
    void genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs);
    void emitCompare(const ir::Expr &lhs, const ir::Expr &rhs, const std::string &jumpIfTrue);
    void genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalNot(const ir::Expr &rhs);
    void genMul(const ir::Expr &lhs, const ir::Expr &rhs);
    void genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder);
    void genShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft);
    void genIndex(const ir::Expr &e);         // slot[idx] read
    void genAddrOf(const ir::Expr &operand);  // &slot or &slot[idx]

    // Leaves slot[idxExpr]'s own effective address in `A0` - see this
    // file's own header comment on why that never needs saving across
    // an *ordinary* nested subexpression, only one that itself might
    // compute another address into `A0`.
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
    std::vector<std::string> trailer_;  // "label: dw v0, v1, ..." data lines, emitted once at the end
    std::map<std::string, std::string> perFileAsm_;

    int tempCounter_ = 0;
    int labelCounter_ = 0;
};

}  // namespace pcc
