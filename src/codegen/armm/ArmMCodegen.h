#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <sstream>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace pcc {

// Generates ARM Thumb assembly (this project's own dialect - see
// ArmMAssembler.h) for the IR built by IRBuilder, targeting the
// ARMv7-M "mainline" M-profile architecture (Cortex-M3/M4/M7 - see
// main.cpp's own makeBackend() comment on the `cortex-m4`/`cortex-m7`
// software-identical aliases this same class answers to, same
// `--target=8088`/`--target=r4000` reasoning as those). Every
// instruction this backend ever emits is real Thumb/Thumb-2 (16-bit
// Thumb-1 ALU/branch/load-store/push-pop ops, plus the handful of
// 32-bit Thumb-2 forms - `MOVW`/`MOVT`, `BL`, the wide `B` this
// backend's own conditional-branch expansion uses - that genuinely
// need Thumb-2's extra width), so it runs unchanged on any real
// ARMv7-M core. Baseline M-profile (Cortex-M0/M0+/M23, ARMv6-M) isn't
// covered: that architecture has neither `MOVW`/`MOVT` (no way here
// to load a 32-bit constant/address without them - a literal pool
// would be the alternative, not implemented) nor hardware `SDIV`
// (moot here anyway - see genDivMod()'s own comment, this backend
// never uses `SDIV` even on cores that have it).
//
// Like M68kCodegen/MipsCodegen (the closest templates - a real
// register machine with a real hardware stack and hardware
// multiply), `generate()` returns a complete, self-contained program
// loaded at a fixed address (`kBaseAddress`, see ArmMCodegen.cpp) -
// no vector table, no reset exception, no boot ROM (the simulator
// sets the CPU's PC there directly, the same "no OS, PC-forced"
// convention 68000/MIPS already use - unlike MC68328Codegen's own
// real-reset-vector-table choice, not needed here since nothing here
// targets real hardware/qemu's own Cortex-M boot sequence), halting
// in a self-branch infinite loop.
//
// This language's own `int` is 32 bits here (this target's native
// register width). Register assignment (this backend's own
// convention, not a real calling-convention choice - every
// parameter is a statically allocated slot, same as every other
// backend):
//   r0 - "ac", genExpr()'s own result (68000's D0, MIPS's $t0) - also
//        where MULS's own 2-operand form naturally wants its
//        destination, and where a real ARM calling convention would
//        put a return value too, so this doubles as a happy
//        coincidence rather than a real ABI decision
//   r1 - "the other operand" in a binary op (68000's D1, MIPS's $t1)
//   r2 - holds a computed address when dereferencing one
//        (array/pointer indexing, `*`/`&`) - 68000's A0, MIPS's $t2
//   r3 - scratch for a comparison's own materialized boolean and for
//        genDivMod()'s own software loop
//   sp - the real hardware stack pointer: real `PUSH`/`POP` (one
//        register at a time - see ArmMAssembler.h's own header
//        comment on why there's no real `{reglist}` syntax here),
//        used both to save one operand across evaluating the other
//        in a binary expression and (via the dedicated `pushlr`/
//        `poppc` mnemonics) for every function call/return
//   lr - the real hardware link register `BL` sets; every function
//        here saves it with `pushlr` on entry and returns via
//        `poppc` (`POP {pc}` - a real, single-instruction ARM
//        return idiom: the saved `lr` value loads straight into
//        `pc`), the same "every function saves/restores its own
//        link register" discipline MipsCodegen.cpp's own comment
//        explains (ARM has exactly one link register too - a nested
//        call would clobber it otherwise)
//
// ARM has no absolute-addressing memory operand (same situation as
// MIPS - see MipsCodegen.h's own header comment): every load/store
// goes through a base register plus an immediate displacement, so
// reading or writing any global/local/array slot first loads that
// slot's own fixed address into `r2` via `MOVW`+`MOVT` (this
// dialect's own address-loading pair - MipsCodegen.h's `LUI`+`ORI`
// equivalent; ArmMAssembler.cpp resolves a label operand to its
// lower/upper 16 bits respectively for these two mnemonics), then
// `ldind`/`stind r2,r0` (this backend's own dedicated mnemonics for
// `LDR`/`STR` with a fixed zero displacement - same convention as
// MIPS's own `ldind`/`stind`).
//
// `*` uses the real hardware `MULS` (Thumb-1's own 2-operand form,
// universal across every M-profile core including baseline) - a
// full 32x32->32 product, matching this language's own `int` width
// exactly, no operand-size limitation at all (unlike the 68000's own
// `MULS.W` - see M68kCodegen.h's own comment).
//
// `/`/`%` are a from-scratch *software* routine (see genDivMod()'s
// own comment) - not real hardware `SDIV`/`UDIV`: those exist on
// ARMv7-M mainline but not on baseline M-profile, and this backend
// would rather have one `/`/`%` implementation that's correct on
// every M-profile core (same reasoning pdp7/6502 already use for
// their own software multiply/divide, just for a different missing-
// hardware reason - see their own comments) than a hardware
// fast path plus a software fallback. It's a plain repeated-
// subtraction loop on absolute values with the sign reapplied
// afterward - correct for any operands, but slow (proportional to
// the quotient's own magnitude) rather than the handful of cycles a
// real `SDIV` or even a binary restoring-division loop would take;
// acceptable the same way 68000's own 16-bit-divisor limitation is -
// every value in this project's own test programs is small.
//
// `<<`/`>>` use the real hardware register-form shift (Thumb-1's own
// 2-operand `LSLS`/`LSRS Rdn,Rm` - the shift *amount* register) -
// `>>` is logical/unsigned (`LSRS`, not the arithmetic `ASRS`), the
// same convention every other backend's own `>>` already uses.
// Multiplying an index by 4 (this target's own word size) instead
// uses the fixed-count immediate form (`lsli ...,2`), needing no
// extra register.
//
// Comparisons use real `CMP` + a materializing branch, the same
// shape as 68000's own `CMP.L`+`Bcc` (ARM's own condition codes
// already encode signed less-than/greater-than correctly in
// hardware, same as 68000's) - see genCompare()'s own comment.
//
// Every conditional branch this backend ever emits
// (`beq`/`bne`/`blt`/`ble`/`bgt`/`bge`) is, in the real machine code
// ArmMAssembler.cpp produces, actually *two* real Thumb instructions:
// a short inverted-condition `Bcc` (always a fixed, tiny ±2-byte
// displacement - just far enough to skip over the instruction right
// after it) followed by a real wide, unconditional `B` with the
// genuine ±16MB range - see ArmMAssembler.cpp's own
// `encodeConditionalBranch()` comment. This sidesteps ever needing
// the trickier (and, for a from-scratch assembler with no way to
// cross-check against a real toolchain, higher-risk-to-get-wrong)
// 32-bit wide conditional-branch encoding, at the cost of 2 bytes
// per conditional branch versus a real compiler's own best case -
// the same kind of pragmatic, documented tradeoff as the 6502
// backend's own out-of-range-branch workaround (see its own
// `safeBranch()` comment), just applied unconditionally here instead
// of only when actually needed.
class ArmMCodegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "cortex-m3"; }
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
    // Software restoring-by-repeated-subtraction divide/remainder -
    // see this file's own header comment on why there's no hardware
    // fast path.
    void genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder);
    void genIndex(const ir::Expr &e);         // slot[idx] read
    void genAddrOf(const ir::Expr &operand);  // &slot or &slot[idx]

    // Leaves slot[idxExpr]'s own effective address in `r2`.
    void genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr);

    // Loads `lbl`'s own address into register `reg` (MOVW+MOVT - see
    // this file's own header comment on why ARM needs two
    // instructions here where 68000 needs none).
    void loadAddress(const std::string &reg, const std::string &lbl);
    // Loads an arbitrary 32-bit compile-time constant into `reg` -
    // `MOVS` alone if it fits in 8 bits, MOVW+MOVT otherwise.
    void loadImm32(const std::string &reg, uint32_t value);

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
