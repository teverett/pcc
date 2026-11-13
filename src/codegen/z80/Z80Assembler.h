#pragma once

#include <cstdint>
#include <string>

#include "fileformat/ObjectFile.h"

namespace rcc {

// Custom COFF machine ID; no official Z80 machine ID exists.
constexpr uint16_t kZ80CoffMachine = 0x5A38;  // ASCII "Z8"

// Must match Z80Codegen.cpp's kBaseAddress, so a linked program loads at
// the same address as a single-file `--format=raw` one.
constexpr uint32_t kZ80BaseAddress = 0x8000;

// Only relocation kind needed: every Z80 jump/call is a full 16-bit
// absolute address.
constexpr uint16_t kZ80RelocAbs16 = 1;

// Two-pass, in-process assembler for the subset of real Z80 assembly this
// backend's codegen emits (no macros, no conditional assembly), plus `in`/
// `out` (`IN A,[n]`/`OUT [n],A`) for port-mapped hardware access from an
// `asm{}` block, since no codegen needs those but a target like
// src/rt/coleco/ (whose VDP/controller/sound are entirely port-mapped,
// unlike memory-mapped TRS-80/Apple II hardware) does. Produces real,
// standard Z80 opcodes (including the CB- and ED-prefixed groups), valid
// input for any accurate Z80 (or 8080, for shared opcodes) emulator or
// disassembler.
//
// `[expr]` always means "the 16-bit address expr names" (e.g. `ld a, [bc]`
// loads through the address in bc), distinguishing a memory reference from
// a register pair's bare value - unlike the 8080 dialect, which rejects
// bracketed operands outright.
//
// An instruction's size is fixed by its mnemonic alone, never by an
// operand's resolved value.
//
// Throws std::runtime_error on any assembly error.
std::string assembleZ80(const std::string &asmText);

// Entry point for separate compilation (--format=coff/elf/macho/aout).
// Assembles like assembleZ80(), except an undefined symbol becomes a
// relocation instead of a hard error, and every address reference also
// gets a relocation against the reserved pseudo-symbol ".." (this
// object's eventual linked base), since labels here resolve relative to
// `org 0` until a linker adds that base in.
ObjectFile assembleZ80Object(const std::string &asmText);

}  // namespace rcc
