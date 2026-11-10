#pragma once

#include <cstdint>
#include <string>

#include "fileformat/ObjectFile.h"

namespace rcc {

// Custom, non-registered COFF/object-format machine ID for this project's
// AArch64 dialect, checked by Arm64LinkerFactory.cpp against `--target=armv8a`.
constexpr uint16_t kArm64CoffMachine = 0xAA64;

// Where the whole program loads and starts running for `--format=raw`,
// and the base Arm64LinkerFactory.cpp's sequential object layout starts from.
constexpr uint64_t kArm64BaseAddress = 0x40080000ull;

// This dialect's three relocation kinds, patched by Arm64LinkerFactory.cpp:
//   Rel26  - `b`/`bl`'s 26-bit word-granularity PC-relative branch field.
//   MovZLo - `movz`'s 16-bit immediate: the low 16 bits of a full address.
//   MovKHi - `movk`'s 16-bit immediate: the high 16 bits.
constexpr uint16_t kArm64RelocRel26 = 1;
constexpr uint16_t kArm64RelocMovZLo = 2;
constexpr uint16_t kArm64RelocMovKHi = 3;
constexpr uint16_t kArm64DarwinMachine = 0xAA65;
constexpr uint16_t kArm64DarwinRelocBranch26 = 4;
constexpr uint16_t kArm64DarwinRelocPage21 = 5;
constexpr uint16_t kArm64DarwinRelocPageoff12 = 6;

// An in-process two-pass assembler for the narrow AArch64 ("A64") dialect
// Arm64Codegen.cpp emits, not a general-purpose A64 assembler. Parses via
// the shared grammar ../../../grammar/RccAsm.g4. Every operand is a plain
// expression (a number, register name, or label); every load/store goes
// through the dedicated `ldind`/`stind` mnemonics instead of a bracketed
// memory operand, always a fixed zero displacement.
//
// Register names recognize both the 32-bit ("W") and 64-bit ("X") view of
// each of the 31 general registers (`w0`..`w30`, `x0`..`x30`) plus
// `wzr`/`xzr` and `sp` - real A64 assembly syntax. Which view names a
// given instruction's destination operand selects that instruction's
// 32-bit-vs-64-bit ("sf") encoding, as a real A64 assembler infers it.
// `ldind`/`stind`'s base-register operand is always 64-bit regardless of
// its `w`/`x` spelling, since real A64 load/store base registers always are.
//
// `cset<cc>` is spelled as six fixed, single-operand mnemonics
// (`csetlt`/`csetgt`/`csetle`/`csetge`/`cseteq`/`csetne`) rather than one
// `cset rd,cc` taking a condition-code operand, since this grammar's
// `expr` has no way to write a bare condition mnemonic that isn't itself
// a register name or a label.
//
// Only the ARMv8-A "A64" 32-bit data-processing instruction subset is
// emitted wherever the destination uses a `w`-register; every
// instruction is a real, standard A64 encoding and a fixed 4 bytes. A64
// branches have no delay slot.
//
// Throws std::runtime_error on any assembly error.
std::string assembleArm64(const std::string &asmText);

// The separate-compilation entry point (`--format=coff`/`elf`/`macho`/
// `aout`): assembles `asmText` (generated with `org 0`) like
// assembleArm64() does, except an undefined symbol is deferred as a
// relocation instead of a hard error, and every absolute-address
// reference - even to a label defined in this same file - also gets a
// relocation against the reserved pseudo-symbol ".." (this text's labels
// resolve to "offset from org 0" until that base is added). Returns the
// format-neutral ObjectFile a codegen backend builds once per compile.
ObjectFile assembleArm64Object(const std::string &asmText, bool darwinHosted = false);

}  // namespace rcc
