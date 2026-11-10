#pragma once

#include <cstdint>
#include <string>

#include "fileformat/ObjectFile.h"

namespace rcc {

// Custom machine ID (no real one exists for the 8008) used by I8008LinkerFactory.cpp to check a linked object's target.
constexpr uint16_t kI8008CoffMachine = 0x8008;

// Base address for both whole-program and object-mode compiles (`org 0`); not a hardware reset-vector requirement.
constexpr uint32_t kI8008BaseAddress = 0;

// `jmp`/`cal` take a full 16-bit absolute target (kI8008RelocAbs16); since the 8008 has no 16-bit immediate load,
// pointAt()'s `mvihi`/`mvi` pair loads a named cell's address one byte at a time, needing its own Lo/Hi relocations.
constexpr uint16_t kI8008RelocAbs16 = 1;
constexpr uint16_t kI8008RelocAbs16Lo = 2;
constexpr uint16_t kI8008RelocAbs16Hi = 3;

// In-process two-pass assembler for the subset of real Intel 8008 assembly I8008Codegen.cpp emits (no macros,
// conditional assembly, or `--format=coff` support). Uses the 8008's real register codes: a=0,b=1,c=2,d=3,e=4,h=5,
// l=6,m=7 (different from the 8080/Z80's numbering). Parses via the shared grammar/RccAsm.g4; supports `name = expr`
// equates (Line::Equate) to pin fixed memory cells to addresses the test harness's Python 8008 emulator can read
// directly. Every instruction's size is fixed by its mnemonic alone, never by an operand's resolved value.
// Throws std::runtime_error on any assembly error.
std::string assembleI8008(const std::string &asmText);

// Separate-compilation entry point (`--format=coff`/`elf`/`macho`/`aout`); mirrors
// ../z80/Z80Assembler.h's assembleZ80Object contract (undefined symbols become relocations, local addresses also
// relocated against the reserved pseudo-symbol ".."). The `name = expr` equates are compile-time constants in every
// object, so they never need a `..` relocation.
ObjectFile assembleI8008Object(const std::string &asmText);

}  // namespace rcc
