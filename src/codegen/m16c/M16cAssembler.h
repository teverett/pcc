#pragma once

#include <cstdint>
#include <string>

#include "fileformat/ObjectFile.h"

namespace pcc {

// This project's own non-registered COFF machine ID for the M16C; checked by
// M16cLinkerFactory.cpp against `--target=m16c`.
constexpr uint16_t kM16cCoffMachine = 0x16C3;

// Load/start address for a whole program; must match M16cCodegen.cpp's
// raw-format `kBaseAddress` so a linked program behaves like a single-file one.
constexpr uint32_t kM16cBaseAddress = 0x0400;

// The one relocation kind this dialect needs: a little-endian 16-bit absolute
// patch, since every jump/call here always encodes a full absolute address.
constexpr uint16_t kM16cRelocAbs16 = 1;

// A from-scratch two-pass assembler for the subset of M16C assembly
// M16cCodegen.cpp emits - no macros, conditional assembly, or relocations.
// Its byte encoding is this project's own invention (not real M16C object
// code), but every mnemonic is a real M16C instruction used for its real
// hardware effect.
//
// Every jump/conditional jump always takes a full absolute address operand
// (unlike real M16C short PC-relative conditional jumps), so there is no
// branch-range limit to work around.
//
// Parses via the shared grammar ../../../grammar/PccAsm.g4; a register name
// is syntactically just an identifier, so this file decides per operand
// whether an identifier names a register or a label (see classifyOperand()
// in M16cAssembler.cpp).
//
// An instruction's size depends on which addressing mode each operand
// resolves to (a register operand is smaller than an immediate/absolute
// one), so this needs two passes for labels to resolve correctly.
//
// Throws std::runtime_error on any assembly error.
std::string assembleM16c(const std::string &asmText);

// The separate-compilation entry point (`--format=coff`/`elf`/`macho`/
// `aout`): assembles `asmText` (generated with `org 0`, not `kBaseAddress`)
// like assembleM16c(), except an undefined symbol becomes a relocation
// instead of a hard error, and every address reference also gets a
// relocation against the reserved pseudo-symbol ".." (this object's own
// eventual linked base, resolved only by a linker).
ObjectFile assembleM16cObject(const std::string &asmText);

}  // namespace pcc
