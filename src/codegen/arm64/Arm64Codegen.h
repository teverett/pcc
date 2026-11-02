#pragma once

#include <map>
#include <optional>
#include <sstream>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace pcc {

// Generates AArch64 assembly (this project's own dialect - see
// Arm64Assembler.h) for the IR built by IRBuilder, targeting the
// ARMv8-A "A64" instruction set every real A-profile core since the
// Cortex-A53 implements (this backend's own test harness runs it on
// qemu's `cortex-a53` model - see tests/run_tests.sh's own comment).
// `--target=armv8a`. Like M68kCodegen/MipsCodegen (the closest
// template here - a real register machine with a real hardware
// stack, hardware multiply/divide/shift, and real signed compare),
// `generate()` returns a complete, self-contained program with no OS
// and no bootloader - the test harness loads it as a raw qemu
// `-kernel` image on the `virt` machine (which places/enters a raw
// image at a fixed, well-documented offset - see MipsCodegen.h's own
// header comment on why a from-scratch simulator wasn't used instead:
// unlike MIPS, qemu ships several real Cortex-A cores, so there's no
// reason not to use one) - halting in a self-branch infinite loop,
// the same "no stack, no recursion" model every backend shares even
// though real function calls (`bl`/`ret`) and a real hardware stack
// (`sp`) are both used.
//
// This language's own `int` is 32 bits here (every real A64 GPR is 64
// bits wide, but every data-processing instruction this backend emits
// is the 32-bit "W" variant - matching every other backend's own
// word-size choice: MIPS's 32 bits, 68000's 32 bits, 6502/8086's 16
// bits). Addresses, though, genuinely need all 64 bits in the A64
// state (there's no 32-bit addressing mode at all, unlike AArch32) -
// so this backend's own `ac`/`other operand`/`addr` registers below
// are each really one physical register addressed two different ways
// depending on role, exactly as real AArch64 code does: `w0` when
// `x0` holds a data value, `x0` when the same register holds an
// address. This isn't ambiguous or unsafe: *every* 32-bit ("W-form")
// instruction here zero-extends its own result into the full 64-bit
// register as an architectural guarantee (ARM DDI 0487, "when the
// destination size is 32 bits, the upper 32 bits of the destination
// register are set to zero") - so a value this backend just computed
// via a `w`-form op is always already safe to read back as its own
// `x`-form view with no explicit extension step, and genAddrOf()/
// genIndex() below rely on exactly that guarantee instead of
// inserting one.
//
// Register assignment (this backend's own convention, not a real
// AAPCS64 calling-convention choice, since there's no cross-function
// register-passed-argument convention at all - every parameter is a
// statically allocated slot, same as every other backend - see
// genFunction()):
//   w0/x0  ("ac")   - genExpr()'s own result (same role as MIPS's $t0)
//   w1/x1           - "the other operand" in a binary op ($t1)
//   w2/x2           - holds a computed address when dereferencing one
//                      (array/pointer indexing, `*`/`&`) ($t2)
//   sp              - the real hardware stack pointer: adjusted with
//                      `add`/`sub sp,sp,#imm`, `ldind`/`stind` through
//                      it, used both to save one operand across
//                      evaluating the other in a binary expression
//                      and for every function call/return
//   x30 (lr)        - the real hardware link register `bl` sets and
//                      `ret` reads
//
// Like MIPS, A64 has no absolute-addressing memory operand - every
// load/store goes through a base register with (here, always) a zero
// displacement, so reading or writing any global/local/array slot
// first loads that slot's own fixed address into `x2` via `movz`+
// `movk` (Arm64Assembler.cpp resolves a label operand to its own
// low/high 16 bits respectively for exactly these two mnemonics - see
// its own header comment), then `ldind`/`stind x2,w0` (this backend's
// own dedicated mnemonics for `ldr`/`str` with a fixed zero
// displacement, same convention as MIPS's own `ldind`/`stind`).
//
// `*`/`/` use the real hardware `sdiv` (a full 32-bit signed divide)
// and `msub` (remainder = dividend - quotient*divisor, in one
// instruction - no packed HI/LO register pair to unpack the way
// MIPS's own `mflo`/`mfhi` need, and no operand-size limitation the
// way 68000's `DIVS.W` has).
//
// `<<`/`>>` use the real hardware variable-count register-form shift
// (`lslv`/`lsrv`) - `>>` is logical/unsigned, same convention every
// other backend's own `>>` already uses. Multiplying an index by 4
// (this target's own word size) instead uses `mul` against a loaded
// constant - see genIndexAddr()'s own comment on why this backend
// has no dedicated shift-by-immediate mnemonic at all.
//
// Comparisons use the real hardware `cmp` (`subs` with a discarded
// result) plus `cset<cc>` (an alias this dialect spells as one of six
// dedicated mnemonics - `csetlt`/`csetgt`/`csetle`/`csetge`/`cseteq`/
// `csetne` - rather than a general `cset rd,cc` taking a condition-
// code operand, since this grammar's own `expr` has no way to spell
// a bare condition mnemonic that isn't itself either a register name
// or a label - see Arm64Assembler.h's own comment) - each one
// directly produces the boolean this language needs (1 or 0), no
// branch at all needed, the same simplicity MIPS's own `slt` family
// already has (see its own header comment) and strictly simpler than
// 68000's CMP+Bcc.
//
// `if`/`while` conditions use `cbz`/`cbnz` (compare-and-branch-if-
// zero/nonzero) directly against `w0` - no flags and no `cmp` needed
// at all for control flow, unlike every flag-based target here.
//
// Unlike MIPS, A64 branches/calls have no delay slot at all - no
// `nop` needed after a `b`/`bl`/`cbz`/`cbnz`.
class Arm64Codegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "armv8a"; }
    std::string sourceExtension() const override { return ".s64"; }
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
    void genExpr(const ir::Expr &e);  // leaves the result in w0

    void genBinary(const ir::Expr &e);
    void genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalNot(const ir::Expr &rhs);
    void genMul(const ir::Expr &lhs, const ir::Expr &rhs);
    void genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder);
    void genIndex(const ir::Expr &e);         // slot[idx] read
    void genAddrOf(const ir::Expr &operand);  // &slot or &slot[idx]

    // Leaves slot[idxExpr]'s own effective address in `x2`.
    void genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr);

    // Loads `lbl`'s own address into 64-bit register `reg` (`movz`+
    // `movk` - see this file's own header comment on why A64 needs
    // two instructions here where 68000 needs none).
    void loadAddress(const std::string &reg, const std::string &lbl);

    static std::string label(const ir::Slot *slot) {
        if (slot->kind == ir::SlotKind::Global) return "g_" + slot->sourceName;
        if (slot->kind == ir::SlotKind::Param) {
            return "p_" + slot->ownerFunctionName + "_" + std::to_string(slot->paramIndex);
        }
        return slot->sourceName + "_" + std::to_string(slot->id);
    }
    static std::string label(const ir::Function *fn) { return "fn_" + fn->sourceName; }

    void declareTemp(const std::string &lbl);
    void declareArray(const std::string &lbl, const std::vector<long> &values);
    std::string newLabel() { return "L" + std::to_string(labelCounter_++); }

    std::ostringstream code_;
    std::vector<std::string> trailer_;  // "label: dw v0, v1, ..." data lines, emitted once at the end
    std::map<std::string, std::string> perFileAsm_;

    int labelCounter_ = 0;
};

}  // namespace pcc
