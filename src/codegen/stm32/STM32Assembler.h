#pragma once

#include <cstdint>
#include <string>

#include "fileformat/ObjectFile.h"

namespace pcc {

// This target's custom COFF machine ID; no registered one exists for this STM32/Cortex-M3 dialect.
constexpr uint16_t kSTM32CoffMachine = 0x3332;

// Flash load address and reset-vector-table location once linked (matches STM32Codegen.cpp's kFlashBase).
constexpr uint32_t kSTM32BaseAddress = 0x08000000u;

// Relocation kinds patched by STM32LinkerFactory.cpp:
//   Abs32     - dlt's 32-bit vector-table entry.
//   MovWLo    - low 16 bits of a movw/movt address expansion.
//   MovTHi    - high 16 bits of the same expansion.
//   Branch25  - bl/b's wide Thumb-2 25-bit PC-relative field (non-contiguous S/J1/J2/imm10/imm11 split).
constexpr uint16_t kSTM32RelocAbs32 = 1;
constexpr uint16_t kSTM32RelocMovWLo = 2;
constexpr uint16_t kSTM32RelocMovTHi = 3;
constexpr uint16_t kSTM32RelocBranch25 = 4;

// In-process two-pass assembler for the narrow Thumb-2/ARMv7-M dialect STM32Codegen.cpp emits.
// Parses via the shared grammar/PccAsm.g4 grammar; its `[expr]` bracket operand maps directly
// onto ARM's `[Rn]` register-indirect syntax.
//
// `mul <Rn>,<Rd>` computes `Rd = Rd * Rn`; `and`/`orr`/`eor`/`mvn <Rd>,<Rm>` compute
// `Rd = Rd <op> Rm` (destructive two-operand Thumb-1 forms) - STM32Codegen.cpp always arranges
// the clobbered operand to be the one it wanted overwritten anyway.
//
// Every branch always uses the 32-bit Thumb-2 "wide" encoding (~1MB range), never the 16-bit
// Thumb-1 form (+/-256 bytes), so no branch-range workaround is needed.
//
// `push`/`pop` take exactly one low register (r0-r7) or the fixed `pushlr`/`poplr` (`push
// {lr}`/`pop {pc}`) - never a multi-register `{reglist}`; the grammar has no `{`/`}` token.
//
// `movl #imm,Rd` and `lea <label>,Rd` both expand to a `movw`+`movt` pair, the only way to get
// an arbitrary 32-bit constant into a register here. `dl`/`dlt`/`ds`/`org`/`ramorg` are data/
// section directives, not instructions: `org` sets the flash address, `ramorg` the RAM address.
// `dl` emits a plain 32-bit flash word (vector table's initial-SP entry). `dlt` emits a label's
// address with the low (Thumb) bit forced to 1 (reset vector's handler entry - required by
// Cortex-M reset logic). `ds <N>` reserves N RAM words with no image bytes (RAM holds nothing
// until startup code writes it). `ramorg <addr>` switches subsequent labels/`ds` into RAM.
//
// Throws std::runtime_error on any assembly error.
std::string assembleSTM32(const std::string &asmText);

// Separate-compilation entry point: assembles `asmText` (generated with `org 0`, not
// kSTM32BaseAddress) like assembleSTM32(), except an undefined symbol is deferred as a
// relocation instead of a hard error, and every flash address reference - even to a label
// defined in this same file - also gets a relocation against the reserved pseudo-symbol ".."
// so the linker can add this object's final flash base.
//
// RAM addresses are exempt from that ".." shift: `ramorg` uses the real fixed hardware RAM
// base unchanged, so a local RAM reference is already final with no relocation needed. An
// external RAM-resident global still becomes an ordinary symbol relocation, but
// GenericLinker's single sequential address space has no notion of a separate RAM region to
// place it against - resolving a cross-object RAM reference is outside this model's scope
// (not exercised by this backend's own single-file test coverage).
//
// Returns the format-neutral ObjectFile a codegen backend builds once per compile.
ObjectFile assembleSTM32Object(const std::string &asmText);

}  // namespace pcc
