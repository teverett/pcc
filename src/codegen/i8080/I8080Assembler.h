#pragma once

#include <cstdint>
#include <string>

#include "fileformat/ObjectFile.h"

namespace pcc {

// This target's own non-registered COFF/object-format machine ID (no real one exists for the
// 8080 in this scheme). Exported so ../../linker/I8080LinkerFactory.cpp can check an input
// object's embedded machine type against `--target=8080`.
constexpr uint16_t kI8080CoffMachine = 0x8080;

// Where the whole program loads and starts running once separate compilation and `--link` are
// both involved; kept identical to I8080Codegen.cpp's own whole-program `kBaseAddress` so a
// linked program behaves exactly like a single-file one.
constexpr uint32_t kI8080BaseAddress = 0x0100;

// This dialect's one relocation kind: a little-endian 16-bit additive patch. Every 8080
// jump/call/16-bit-load this backend emits is a full absolute address, so this is the only
// relocation kind needed.
constexpr uint16_t kI8080RelocAbs16 = 1;

// A from-scratch, in-process two-pass assembler for the subset of real Intel 8080 assembly
// I8080Codegen.cpp emits (no macros, no conditional assembly). Uses real, standard 8080
// mnemonics and produces real 8080 machine code matching Intel's published encoding, so the
// output is valid input to any accurate 8080 (or object-code-compatible Z80) emulator or
// disassembler.
//
// Parses via the shared grammar ../../../grammar/PccAsm.g4; a register name (`a`, `b`, ..., `h`,
// `l`, `m`, `sp`, `psw`) is syntactically just a bare identifier to that grammar, so this file
// decides, per mnemonic and operand position, whether an identifier names a register or a
// symbol to resolve against the label table.
//
// Every instruction's size is fixed by its mnemonic alone, never by any operand's resolved
// value.
//
// Throws std::runtime_error on any assembly error.
std::string assembleI8080(const std::string &asmText);

// The real separate-compilation entry point (`--format=coff`/`elf`/`macho`/`aout`): assembles
// `asmText` (generated with `org 0`, not `kI8080BaseAddress`) like assembleI8080() does, except
// an undefined symbol is deferred as a relocation instead of a hard error, and every address
// reference - even to a label defined in this same file - also gets an additional relocation
// against the reserved pseudo-symbol ".." (this object's eventual linked base, resolved only by
// a linker). Returns the format-neutral ObjectFile a codegen backend builds once per compile;
// this function never touches a real COFF/ELF/Mach-O/a.out byte layout itself.
ObjectFile assembleI8080Object(const std::string &asmText);

}  // namespace pcc
