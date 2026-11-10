#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace pcc {

// Generates TMS320C6713 (TI C67x VLIW DSP) assembly for the IR built by IRBuilder. Real C6000
// hardware packs several instructions into one "execute packet" (VLIW), reads either register
// file cross-path, and runs two sides (A/B) in parallel; this backend deliberately uses none of
// that: every instruction's own `p` bit is 0 (it always ends its own execute packet, so no two
// instructions this backend emits ever issue in the same cycle), every instruction's `s` bit is
// 1 and every operand register is a B-file register B0..B15 (never A0..A15), and the cross-path
// bit `x` is therefore always 0. This is a real, always-correct subset of the actual ISA -
// deliberately leaving VLIW scheduling/cross-path reads unexploited, the same kind of
// simplification SparcCodegen/PowerPCCodegen already make for their own targets - never an
// invented or approximate encoding (see Tms320C6713Assembler.h's own header comment).
//
// Register convention (an invented allocation within the real ABI's own fixed roles for B3/
// B14/B15 - not real TI ABI register-usage conventions beyond those three):
//   B0  - genExpr()'s result ("ac"); also the sole predicate register for every conditional
//         branch this backend emits (see "Conditional branches" below)
//   B1  - the other operand in a binary op (like Alpha's t1)
//   B2  - scratch register for a computed address when dereferencing one (like Alpha's t2)
//   B4..B12 - further scratch (mirror how Alpha uses t3-t7/at)
//   B13 - always holds the constant 0 (materialized once in the entry sequence and never
//         written again anywhere else in generated code) - a `mov` is `ADD dst,src,B13` the
//         same way Alpha's own dialect uses its hardwired `zero` register
//   B14 - this call's frame pointer (fp)
//   B15 - hardware stack pointer (sp) - matches the real TI C6000 ABI's own use of B15 as SP
//   B3  - the link/return-address register - matches the real TI C6000 ABI's own use of B3 as
//         the return-address register for a call
//
// Frame layout relative to B14 (fp), same shape as AlphaCodegen.h's own: `fp+0` is the caller's
// saved fp, `fp+4` the saved return address (see genFunction()'s own comment on the save/move
// ordering), a Param's address is `fp+frameOffset` (caller-pushed, right-to-left, caller
// cleanup), a Local's is `fp-localLowOffset(...)`.
//
// This ISA has no absolute-addressing/displacement memory operand this backend ever uses -
// every load/store address (Local/Param/Global, or any non-zero byte offset at all) is always
// built into a scratch register first via MVK/MVKH (+ ADD when a base register is also
// involved), then issued through the D-unit format's small-nonnegative-in-range-offset mode
// with offsetR always 0 (see Tms320C6713Assembler.h's own comment on why that's always valid,
// never just the fallback this real ISA's own assembler would reserve for an out-of-range
// case).
//
// Real pipeline hazards modeled with explicit NOP padding (no instruction scheduling at all -
// simplicity over performance, same philosophy as every other backend here, exactly mirroring
// MipsCodegen.cpp's own single-NOP branch-delay habit, just far more of it): every `B`
// (register or constant form) is followed by exactly 5 NOPs; every `LDB`/`LDH`/`LDW` by exactly
// 4; every `MPY`/`MPYH`/`MPYHL`/`MPYLH` by exactly 1. See emitBranch()/emitLoad()/emitMul().
//
// Conditional execution: every instruction format carries a `creg`/`z` predicate field; this
// backend only ever uses `creg==0` (always execute - every non-branch instruction, and every
// unconditional branch) or `creg==1` (predicate on B0 - the only conditional branches this
// backend emits: BZ branches if B0==0, BNZ if B0!=0). `if`/`while` compute their test into B0
// via a CMP* instruction (exactly like Alpha's genCompare() into t0), then BZ/BNZ the same way
// Alpha's own genStmt() uses `beq`/`bne`.
//
// `int` and `long` are both the real TI C6000 ABI's 32-bit native word; only `long long` is
// wide and needs a register pair (lo in B0, hi in B1 - the same "result occupies two adjacent
// scratch registers" convention MipsCodegen.h documents for its own 32-bit-native `long long`
// handling, not an invented scheme). There is no hardware integer divide at all, for any width
// (a genuine, real C6000 limitation, fixed- and floating-point alike) - `/`/`%` always run a
// bit-serial restoring-division software loop (genDivModCore() for the 32-bit case, directly
// mirroring AlphaCodegen.cpp's own genDivModCore(); genWideDivMod() for `long long`, directly
// mirroring MipsCodegen.cpp's own register-pair genWideDivMod()). There is also no native
// 32x32->32 multiply instruction (only 16x16->32 MPY/MPYH/MPYHL/MPYLH) - a plain `int*int`
// always runs the standard TI 16x16-decomposition sequence (genMulLow32(); see its own
// comment); `long long` multiply additionally needs the low*low cross term's own full 64-bit
// product, gotten via a 32-iteration shift-and-add loop (genMulFull32()) rather than trying to
// extend the sign-extending MPY family to an unsigned 64-bit partial product no instruction
// here actually computes.
class Tms320C6713Codegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "tms320c6713"; }
    TypeWidths typeWidths() const override { return {1, 2, 4, 4, 8}; }
    std::string sourceExtension() const override { return ".stms6713"; }
    // Whole-program flat binary only; see this header's own comment on why this first pass
    // never implements real separate compilation (no relocatable object format) for this
    // target.
    std::vector<std::string> assembledFormats() const override { return {"raw"}; }
    std::map<std::string, std::string> perFileAsm() const override;
    std::optional<std::string> assembleInProcess(const std::string &asmText,
                                                  const std::string &format) const override;

   private:
    void genFunction(const ir::FunctionBody &fn);
    void genEpilogue();
    void genStmt(const ir::Stmt &s);
    void genExpr(const ir::Expr &e);  // leaves the result in B0 (B1:B0 if wide)

    void genBinary(const ir::Expr &e);
    void genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs);
    void genWideCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned);
    void genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalNot(const ir::Expr &rhs);
    void genMul(const ir::Expr &lhs, const ir::Expr &rhs);
    void genWideMul(const ir::Expr &lhs, const ir::Expr &rhs);
    void genWideAdd(const ir::Expr &lhs, const ir::Expr &rhs, bool isAdd);
    void genWideShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned);
    void genNegate64();  // negates B1:B0 in place
    void genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned);
    void genDivModCore(bool wantRemainder, bool isUnsigned);  // B0=dividend, B1=divisor on entry
    void genWideDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned);
    void genIndex(const ir::Expr &e);
    void genAddrOf(const ir::Expr &operand);
    void genLocalArrayInit(const ir::Slot *slot);
    // B0=old,B1=rhs on entry; CompoundAssign/CompoundDerefAssign never target a wide type (see
    // IRBuilder.cpp's own rejectIfWide() - checked once there, not re-checked per backend).
    void genCompoundOp(const std::string &op, Type type);
    void genCompoundThroughT2(const ir::Expr &e);

    int widthOfType(Type t) const { return pcc::widthOf(t, typeWidths()); }
    bool isWide(Type t) const { return widthOfType(t) > widthOfType(Type{BaseType::Int, false}); }
    int elemShift(Type t) const {
        switch (widthOfType(t)) {
            case 1: return 0;
            case 2: return 1;
            case 4: return 2;
            default: return 3;
        }
    }

    // Materializes a compile-time-known 32-bit immediate into `reg` (MVK alone when it's
    // already the sign extension of its own low 16 bits, MVK+MVKH otherwise).
    void loadImm32(const std::string &reg, int32_t v);
    // Loads `lbl`'s address into `reg` (MVK+MVKH against the label, always both - the
    // assembler resolves each against the label's actual address; see Tms320C6713Assembler.h).
    void loadAddress(const std::string &reg, const std::string &lbl);
    void loadSlotAddr(const std::string &reg, const ir::Slot *slot);

    // `reg += delta` (`delta` materialized into a scratch register first - there's no
    // immediate-operand ALU instruction on this target at all).
    void adjustReg(const std::string &reg, int32_t delta);
    // `dst = baseReg + byteOffset` (`byteOffset` materialized into `dst` first, then added).
    void computeAddr(const std::string &dst, const std::string &baseReg, int32_t byteOffset);
    // `reg = [SP+byteOffset]` / `[SP+byteOffset] = reg`, via B2 for the address (see
    // genWideDivMod()'s own comment: a stack-relative scratch layout, the same technique
    // MipsCodegen.cpp's own genWideDivMod() uses, just built through computeAddr() since this
    // ISA has no displacement memory operand at all).
    void loadStack(const std::string &reg, int32_t byteOffset);
    void storeStack(const std::string &reg, int32_t byteOffset);

    void genPush(const std::string &reg);
    void genPop(const std::string &reg);
    void genPushWide(const std::string &lo, const std::string &hi);
    void genPopWide(const std::string &lo, const std::string &hi);

    // Emits `mnemonic base,reg` (LDB/LDH/LDW) plus the mandatory 4-NOP load delay.
    void emitLoad(const std::string &mnemonic, const std::string &base, const std::string &reg);
    // Emits `mnemonic base,reg` (STB/STH/STW); no delay.
    void emitStore(const std::string &mnemonic, const std::string &base, const std::string &reg);
    // Emits `mnemonic dst,src1,src2` (MPY/MPYH/MPYHL/MPYLH) plus the mandatory 1-cycle
    // multiply delay.
    void emitMul(const std::string &mnemonic, const std::string &dst, const std::string &src1,
                 const std::string &src2);
    // Emits an unconditional constant-displacement branch plus its mandatory 5-NOP delay.
    void emitBranch(const std::string &label);
    // Emits `BZ`/`BNZ label` (predicated on B0) plus its mandatory 5-NOP delay.
    void emitBranchZ(bool branchIfZero, const std::string &label);
    // Emits a register-indirect branch (`BR (reg)`) plus its mandatory 5-NOP delay.
    void emitBranchReg(const std::string &reg);

    void loadOperand(const std::string &addrReg, Type type);   // -> B0 (B1:B0 if wide)
    void storeOperand(const std::string &addrReg, Type type);  // B0 (B1:B0 if wide) -> memory
    void narrowB0To(Type to);
    void genConvert(const ir::Expr &e);

    void genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr);  // -> B2

    // Low 32 bits of src1*src2 via the standard TI 16x16-decomposition sequence (sign-agnostic
    // - see this header's own comment).
    void genMulLow32(const std::string &dst, const std::string &src1, const std::string &src2);
    // Full unsigned 64-bit product of two 32-bit registers (outLo:outHi), via a 32-iteration
    // shift-and-add loop; used only for the `long long` multiply's low*low cross term, where
    // the carry into bit 32 actually matters (see this header's own comment).
    void genMulFull32(const std::string &aReg, const std::string &bReg, const std::string &outLo,
                       const std::string &outHi);

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
    std::vector<std::string> trailer_;
    std::map<std::string, std::string> perFileAsm_;

    int labelCounter_ = 0;
    std::vector<std::pair<std::string, std::string>> loopLabels_;
    std::map<int, std::string> irLabels_;
};

}  // namespace pcc
