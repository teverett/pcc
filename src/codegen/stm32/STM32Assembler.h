#pragma once

#include <cstdint>
#include <string>

#include "fileformat/ObjectFile.h"

namespace pcc {

// This target's own custom, non-registered COFF/object-format machine
// ID (no real one exists for this project's own STM32/Cortex-M3
// dialect - see ../../fileformat/coff/CoffFormat.h's own header comment) -
// exported so ../../linker/factory/STM32LinkerFactory.cpp can check an
// input object's embedded machine type against `--target=stm32`
// without duplicating this literal. ASCII "32" (`'3'=0x33,'2'=0x32`) -
// this dialect's own distinguishing suffix, since the "STM" prefix is
// shared with stm8 (see STM8Assembler.h's own `kSTM8CoffMachine`,
// which picked an unrelated numeric scheme instead, so there's no
// collision either way).
constexpr uint16_t kSTM32CoffMachine = 0x3332;

// Where this backend's own flash image loads and the reset vector
// table lives once real separate compilation (`--format=coff`/`elf`/
// `macho`/`aout`, see `allowExternal` below) and `--link` are both
// involved - the same address STM32Codegen.cpp's own whole-program
// `--format=raw` entry sequence already uses (its own private
// `kFlashBase`), kept identical so a linked program behaves exactly
// like a single-file one. Exported so STM32LinkerFactory.cpp's own
// sequential object layout starts from the same place.
constexpr uint32_t kSTM32BaseAddress = 0x08000000u;

// This dialect's four relocation kinds - see
// ../../linker/factory/STM32LinkerFactory.cpp's own patch functions
// (`MovWLo`/`MovTHi`/`Branch25` are kept local there, not in the
// shared GenericLinker.h, same precedent as ArmLinkerFactory.cpp's own
// split: classic ARM's `MOVW`/`MOVT` imm4:imm12 split and this
// dialect's own Thumb-2 `movw`/`movt` imm4:i:imm3:imm8 split share the
// same *idea* - an additive 16-bit half of a 32-bit address - but not
// the same bit layout, so each target keeps its own patch function;
// `Abs32` is reused from the shared `RelocKinds::patchAbs32` instead,
// since `dlt`'s own 32-bit field, once resolved, is a plain
// little-endian additive word with no split at all - see
// STM32Assembler.cpp's own `Line::Dlt` comment):
//   Abs32     - `dlt`'s own 32-bit vector-table entry (used only by
//               STM32LinkerFactory.cpp's own synthesized entry
//               object's `dlt _start`, never by ordinary user code -
//               see STM32Codegen.cpp's own `generate()` comment on why
//               the vector table only ever lives there now).
//   MovWLo    - `lea`'s own expansion into `movw`+`movt` (see
//               STM32Assembler.h's own big header comment below): the
//               low 16 bits of the address.
//   MovTHi    - the same expansion's high 16 bits.
//   Branch25  - `bl`/`b`'s own wide Thumb-2 25-bit PC-relative branch
//               field, split non-contiguously across S/J1/J2/imm10/
//               imm11 (see encodeBranch25() below) - never a plain
//               contiguous N-bit integer the way classic ARM's own
//               24-bit `B`/`BL` field is, hence its own dedicated kind
//               rather than reusing ArmLinkerFactory.cpp's `Rel24`.
constexpr uint16_t kSTM32RelocAbs32 = 1;
constexpr uint16_t kSTM32RelocMovWLo = 2;
constexpr uint16_t kSTM32RelocMovTHi = 3;
constexpr uint16_t kSTM32RelocBranch25 = 4;

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

// The real separate-compilation entry point (`--format=coff`/`elf`/
// `macho`/`aout` - see STM32Codegen.h's own `objectMode`): assembles
// `asmText` (generated with `org 0` for its flash section, not
// `kSTM32BaseAddress` - see STM32Codegen.cpp's own `generate()`)
// exactly like assembleSTM32() does, except an undefined symbol is
// deferred as a relocation instead of a hard error, and every *flash*
// address reference - even to a label defined in this same file -
// also gets an additional relocation against the reserved
// pseudo-symbol ".." (see ../../linker/GenericLinker.h's own header
// comment on why: this text's own flash labels all resolve to "offset
// from this object's own org 0" until that base is added).
//
// RAM addresses are deliberately exempt from that same ".." shift:
// `ramorg` keeps using the real fixed hardware base (`kRamBase`, see
// STM32Codegen.cpp) unchanged even here, since RAM is a genuinely
// separate address space whose placement has nothing to do with where
// this object's own code ends up in flash once linked - a *local* RAM
// address reference is therefore already its own final, correct value
// the moment this function resolves it, with no relocation needed at
// all (see STM32Assembler.cpp's own `isRamLabel_` comment). An
// *external* RAM-resident global (declared but not defined in this
// file) still becomes an ordinary symbol relocation like any other
// undefined name - but GenericLinker's own single sequentially-laid-
// out address space has no notion of a second, RAM-based region to
// place such a symbol's real linked value against, so correctly
// resolving a cross-object reference to another file's own RAM-
// resident global is outside what this migration step's linker model
// can do; every reference this backend's own test coverage exercises
// stays within one file (see tests/run_tests.sh's own check_stm32()),
// so this never comes up there.
//
// Returns the format-neutral ObjectFile a codegen backend builds once
// per compile (see ../../fileformat/ObjectFile.h) - this function
// never touches a real COFF/ELF/Mach-O/a.out byte layout itself.
ObjectFile assembleSTM32Object(const std::string &asmText);

}  // namespace pcc
