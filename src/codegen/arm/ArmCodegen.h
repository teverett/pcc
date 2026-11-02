#pragma once

#include <map>
#include <optional>
#include <sstream>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace pcc {

// Generates ARM assembly (this project's own dialect - see
// ArmAssembler.h) for the IR built by IRBuilder, targeting the
// ARMv7-A "A32" (32-bit ARM, not Thumb) instruction encoding common
// to every A-profile core (Cortex-A5/A7/A8/A9/A15/...) -
// `--target=armv7a`. Like M68kCodegen/MipsCodegen (the closest
// templates here - a real register machine with a real hardware
// stack and real signed compare), `generate()` returns a complete,
// self-contained program loaded at a fixed address (`kBaseAddress`,
// see ArmCodegen.cpp) - no OS, no bootloader, the test harness sets
// the simulated CPU's PC there directly (see tests/run_tests.sh's
// own qemu-system-arm invocation) - halting in a self-branch infinite
// loop, the same "no stack, no recursion" model every backend shares
// even though real function calls (`BL`/`BX`) and a real hardware
// stack (`SP`) are both used.
//
// This language's own `int` is 32 bits here, matching this target's
// native register width (every A32 GPR is 32 bits) - same reasoning
// as every other backend's own word-size choice.
//
// Register assignment (this backend's own convention, not a real
// AAPCS calling-convention choice, since there's no cross-function
// register-passed-argument convention at all - every parameter is a
// statically allocated slot, same as every other backend - see
// genFunction()):
//   r0  - "ac", genExpr()'s own result (the register every other
//         backend also calls this - 68000's D0, MIPS's $t0)
//   r1  - "the other operand" in a binary op (68000's D1, MIPS's $t1)
//   r2  - holds a computed address when dereferencing one
//         (array/pointer indexing, `*`/`&`) - 68000's A0, MIPS's $t2
//   r3  - scratch, used only inside genDivMod() to hold a quotient/
//         divisor copy so SDIV/MLS never has a source and destination
//         register collide (see its own comment)
//   sp (r13) - the real hardware stack pointer: SUB/ADD-adjusted,
//              LDR/STR through it (via the dedicated `ldind`/`stind`
//              mnemonics below), used both to save one operand across
//              evaluating the other in a binary expression and for
//              every function call/return
//   lr (r14) - the real hardware link register BL sets and BX reads
//
// ARM has no absolute-addressing memory operand at all (unlike the
// 68000's own "MOVE.L label,D0" - see M68kCodegen.h's own header
// comment) - every load/store goes through a base register plus a
// (here, always zero) offset, so reading or writing any global/local/
// array slot first loads that slot's own fixed address into `r2` via
// `MOVW`+`MOVT` (ArmAssembler.cpp resolves a label operand to its
// lower/upper 16 bits respectively for exactly these two mnemonics -
// same "no %hi/%lo syntax needed" convention as MipsAssembler.h's own
// `lui`/`ori`, see its own header comment), then `ldind`/`stind
// r2,r0` (this backend's own dedicated mnemonics for `LDR`/`STR` with
// a fixed zero offset - same "no bracketed memory operand" convention
// as MIPS/M16C/STM8 - see MipsAssembler.h's own header comment on
// why).
//
// `*`/`/` use the real hardware `SDIV` (ARMv7-A's optional integer-
// divide extension - present on every Cortex-A model qemu offers,
// e.g. `-cpu cortex-a15`, see tests/run_tests.sh's own comment) - a
// full 32-bit signed quotient directly, no HI/LO-register-pair
// unpacking the way MIPS's own `MFLO`/`MFHI` needs. `%` additionally
// uses `MLS` (multiply-subtract, ARMv6T2+) to compute the remainder
// as `dividend - quotient*divisor` in one instruction - see
// genDivMod()'s own comment on why its operands are arranged the way
// they are (avoiding a same-register Rd/Rm multiply, which several
// real ARM cores treat as UNPREDICTABLE).
//
// `<<`/`>>` use the real hardware variable-count shifted-register
// form of `MOV` (this dialect's own `lsl`/`lsr` mnemonics - see
// ArmAssembler.h's own comment) - `>>` is logical/unsigned (`lsr`,
// not the arithmetic `asr`), the same convention every other
// backend's own `>>` already uses (no unsigned type to make the
// distinction with). Multiplying an index by 4 (this target's own
// word size) instead uses the fixed-shift-amount form (`lsl
// rd,rn,#2`) - no extra register needed, unlike the register-count
// form.
//
// Comparisons are where this target is genuinely nicer than every
// earlier one here: `CMP` sets NZCV exactly like the 68000's own
// `CMP`, but instead of needing a real conditional *branch* to
// materialize the 0/1 boolean this language needs (the 68000's own
// `CMP`+`Bcc`+two-branch dance - see M68kCodegen.h's own comment),
// every ARM data-processing instruction (including `MOV`) carries its
// own 4-bit condition field, so "the boolean is 1 iff flags satisfy
// condition X" is just `MOV r0,#0` followed by one *conditionally
// executed* `MOV r0,#1` (`MOVEQ`/`MOVNE`/`MOVLT`/`MOVGE`/`MOVLE`/
// `MOVGT`) - no branch at all, for any of the six operators. See
// genCompare()'s own comment for exactly which condition each
// operator becomes.
class ArmCodegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "armv7a"; }
    std::string sourceExtension() const override { return ".sarm"; }
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
    void genExpr(const ir::Expr &e);  // leaves the result in r0

    void genBinary(const ir::Expr &e);
    void genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalNot(const ir::Expr &rhs);
    void genMul(const ir::Expr &lhs, const ir::Expr &rhs);
    void genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder);
    void genIndex(const ir::Expr &e);         // slot[idx] read
    void genAddrOf(const ir::Expr &operand);  // &slot or &slot[idx]

    // Leaves slot[idxExpr]'s own effective address in `r2`.
    void genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr);

    // Loads `lbl`'s own address into register `reg` (MOVW+MOVT - see
    // this file's own header comment on why ARM needs two
    // instructions here where 68000 needs none).
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
