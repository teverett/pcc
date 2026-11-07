#pragma once

#include <cstdint>
#include <string>

#include "fileformat/ObjectFile.h"

namespace pcc {

// Custom machine ID for this target (no registered COFF/object-format ID exists for the MC68328),
// distinct from m68k's kM68kCoffMachine since objects for the two are never link-compatible: only
// this backend expects the linker to synthesize a reset vector table.
constexpr uint16_t kMC68328CoffMachine = 0x6328;

// Address of `_start` in a whole-program reset-vector image, right after the 2-word vector table;
// irrelevant in object mode, which uses `org 0` and lets the linker place the reset vector table.
constexpr uint32_t kMC68328StartAddress = 8;

// This dialect's only relocation kind: a big-endian 32-bit additive patch. No relative kind is
// needed since Bcc/BRA targets are always intra-file.
constexpr uint16_t kMC68328RelocAbs32 = 1;

// In-process two-pass assembler for the narrow 68000 dialect MC68328Codegen.cpp emits. The
// MC68328 "DragonBall" core executes a bit-for-bit identical instruction set to a plain MC68000,
// so this mirrors ../m68k/M68kAssembler's encoder (operand classification, mnemonic vocabulary,
// fixed size per mnemonic).
//
// Throws std::runtime_error on any assembly error.
std::string assembleMC68328(const std::string &asmText);

// Separate-compilation entry point (`--format=coff`/`elf`/`macho`/`aout`): like
// assembleMC68328(), but an undefined symbol is deferred as a relocation instead of a hard
// error, and every absolute-address reference also gets a relocation against the pseudo-symbol
// ".." (the object's eventual linked base), since labels resolve relative to `org 0` until that
// base is added.
ObjectFile assembleMC68328Object(const std::string &asmText);

}  // namespace pcc
