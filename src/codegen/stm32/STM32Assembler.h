#pragma once

#include <string>

namespace pcc {

// A from-scratch, in-process two-pass assembler for exactly the
// narrow Thumb-2/ARMv7-M dialect STM32Codegen.cpp emits - not a
// general-purpose ARM assembler. Parses via the same shared grammar
// ../../../grammar/PccAsm.g4 every other backend's own in-process
// assembler uses (see M68kAssembler.h's own header comment on its
// shape); unlike that one, this dialect *does* use the grammar's
// `[expr]` bracket operand (`ldr r0,[r2]`/`str r0,[r2]`) for real
// register-indirect addressing - ARM's own `[Rn]` syntax maps onto it
// directly, with no ambiguity against a parenthesized group the way
// 68000's `(An)` has (see that file's own comment on why it needed
// dedicated `ldind`/`stind` mnemonics instead).
//
// Every encoding here is a real ARMv7-M (Cortex-M3) instruction,
// confirmed against `clang --target=thumbv7m-none-eabi`'s own
// integrated assembler + `llvm-objdump` during development - not
// hand-derived from the architecture reference alone. Two real
// instructions are 16-bit Thumb-1 two-operand forms where this
// dialect's own mnemonic only takes two operands even though the
// underlying instruction is a destructive "the ouput reuses one
// input's own register" form (`mul <Rn>,<Rd>` really computes
// `Rd = Rd * Rn`; `and`/`orr`/`eor`/`mvn <Rd>,<Rm>` compute
// `Rd = Rd <op> Rm`) - STM32Codegen.cpp's own calling convention
// always arranges for that to already be the operand it wanted
// clobbered anyway (the same "ac" role every other backend's own
// result register plays), so this is never a real limitation here.
//
// Every branch (`b`/`beq`/`bne`/`blt`/`ble`/`bgt`/`bge`/`bl`) always
// uses the 32-bit Thumb-2 "wide" encoding (a real ~1MB range) rather
// than the 16-bit Thumb-1 form (+/-256 bytes for a conditional
// branch) - see STM32Codegen.h's own header comment on why that means
// no branch-range workaround is ever needed here, the same simplicity
// m68k/mc68328 get from their own real 16-bit-displacement `Bcc`.
//
// `push`/`pop` only ever take exactly one low register (`r0`-`r7`) or
// the fixed `lr`/`pc` forms (`pushlr`/`poplr`, this dialect's own
// spelling of `push {lr}`/`pop {pc}` - the latter doubling as this
// backend's own `return`, restoring `lr` straight into `pc`) - never
// a real multi-register `{reglist}`, since STM32Codegen.cpp never
// needs to save more than one register at a time (see that file's
// own header comment on its three-register convention). The grammar
// has no `{`/`}` token at all, so a real reglist literally couldn't
// be parsed here even if a future caller wanted one.
//
// `movl #imm,Rd` (an arbitrary 32-bit immediate) and `lea <label>,Rd`
// (a label's own resolved address) both expand to a real `movw`+
// `movt` pair - this dialect's only way to get an arbitrary 32-bit
// constant into a register, there being no literal pool/PC-relative
// `ldr Rd,=imm` here. `dl`/`dlt`/`ds`/`org`/`ramorg` are this
// dialect's own data/section directives, not real instructions - see
// STM32Codegen.h's own header comment on why this backend needs two
// separate address spaces (`org` for flash, `ramorg` for RAM) where
// every other flat-binary backend here only ever needs one: `dl`
// emits a plain 32-bit flash word (used only for the reset vector
// table's own initial-SP entry), `dlt` emits a label's resolved
// address with its low (Thumb) bit forced to 1 (the reset vector
// table's own reset-handler entry - confirmed directly against
// qemu's own Cortex-M reset logic that this bit is genuinely required,
// not just a convention real toolchains happen to follow: omitting it
// faults immediately, see tests/run_tests.sh's own check_stm32()),
// `ds <N>` reserves `N` RAM words for one label with no image bytes
// at all (RAM holds no useful content until this backend's own
// startup code writes it - see STM32Codegen.cpp's own `genGlobalInit`
// comment), and `ramorg <addr>` switches every following label's own
// address (and every following `ds`) into that RAM space, never
// emitting into the flash image's own byte stream from that point on.
//
// Throws std::runtime_error on any assembly error.
std::string assembleSTM32(const std::string &asmText);

}  // namespace pcc
