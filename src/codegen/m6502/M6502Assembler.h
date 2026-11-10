#pragma once

#include <cstdint>
#include <string>

#include "fileformat/ObjectFile.h"

namespace rcc {

// No real COFF machine ID exists for the 6502; this is a project-local stand-in.
constexpr uint16_t kM6502CoffMachine = 0x6502;

// Linked base address; matches the whole-program raw entry's kBaseAddress.
constexpr uint32_t kM6502BaseAddress = 0x0600;

// Three reloc kinds because LDA takes only an 8-bit immediate: loading a
// 16-bit address into a register pair needs separate low/high-byte patches.
constexpr uint16_t kM6502RelocAbs16 = 1;
constexpr uint16_t kM6502RelocAbs16Lo = 2;  // low byte only (value & 0xFF)
constexpr uint16_t kM6502RelocAbs16Hi = 3;  // high byte only (value >> 8)

// In-process two-pass assembler for the narrow 6502 dialect M6502Codegen.cpp
// emits, not a general-purpose 6502 assembler. No register operand syntax
// (each mnemonic implies its own register) and no bracketed memory operands
// (zero-page indirect indexed uses dedicated `ldaind`/`staind` mnemonics).
// Every instruction's size is fixed by its mnemonic alone, independent of
// operand values, so a stable two-pass size computation needs no fixup pass.
//
// Throws std::runtime_error on any assembly error.
std::string assembleM6502(const std::string &asmText);

// Separate-compilation entry point: assembles with `org 0`, defers undefined
// symbols as relocations, and emits a relocation against pseudo-symbol ".."
// for every local address reference (resolved once linked). Returns a
// format-neutral ObjectFile; this function never writes real object bytes.
ObjectFile assembleM6502Object(const std::string &asmText);

}  // namespace rcc
