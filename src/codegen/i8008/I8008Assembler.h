#pragma once

#include <cstdint>
#include <string>

#include "fileformat/ObjectFile.h"

namespace pcc {

// This target's own custom, non-registered COFF/object-format machine
// ID (no real one exists for the 8008 - see ../../fileformat/coff/CoffFormat.h's
// own header comment) - exported so
// ../../linker/factory/I8008LinkerFactory.cpp can check an input
// object's embedded machine type against `--target=8008` without
// duplicating this literal.
constexpr uint16_t kI8008CoffMachine = 0x8008;

// This backend's whole-program entry sequence already uses `org 0`
// (see I8008Codegen.cpp's own `generate()`) - object-mode compiles
// use the exact same `org 0` for the same reason every other
// GenericLinker-based target's own object does (see
// Z80Assembler.h's own comment), and I8008LinkerFactory.cpp's own
// sequential object layout starts from this same address too, purely
// for consistency with the existing whole-program convention (there's
// no real hardware reason this couldn't be any other address - the
// 8008 itself has no reset-vector convention this project models).
constexpr uint32_t kI8008BaseAddress = 0;

// This dialect's three relocation kinds - mirroring
// ../m6502/M6502Assembler.h's own `kM6502RelocAbs16`/`Lo`/`Hi` exactly
// and for the identical reason: `jmp`/`cal` (and every conditional
// jump) take a real, full 16-bit absolute target (`kI8008RelocAbs16`
// - see I8008Assembler.cpp's own `emit16`), but this chip has no real
// 16-bit immediate load at all (see I8008Codegen.h's own header
// comment) - `pointAt()`'s own `mvihi`/`mvi` pair loads a named
// cell's address one byte at a time, so a *named* cell's address
// (including an external global's) needs its own byte-at-a-time
// relocation too (`kI8008RelocAbs16Lo`/`Hi`).
constexpr uint16_t kI8008RelocAbs16 = 1;
constexpr uint16_t kI8008RelocAbs16Lo = 2;
constexpr uint16_t kI8008RelocAbs16Hi = 3;

// A from-scratch, in-process two-pass assembler for the subset of
// real Intel 8008 assembly I8008Codegen.cpp emits - not a general-
// purpose 8008 assembler (no macros, no conditional assembly, no
// relocations/external symbols/`--format=coff` - see
// I8008Codegen.h's own header comment: this backend only supports a
// single, fully self-contained `--format=raw` compile).
//
// Uses real, standard mnemonics and the 8008's own real register
// codes (a=0,b=1,c=2,d=3,e=4,h=5,l=6,m=7 - *not* the same numbering
// the 8080/Z80 backends' own assemblers use, see I8008Codegen.h's own
// header comment) and produces real 8008 machine code, verified
// against Intel's own published instruction set (Bryan Blackburn's
// 8008 Instruction Set code-sort reference was used to check every
// opcode byte this file emits).
//
// Parses via the same shared grammar ../../../grammar/PccAsm.g4 every
// other backend's own in-process assembler uses; a register name is
// otherwise just another bare identifier to that grammar - this file
// decides, per mnemonic and operand position, whether a given
// identifier names a register or a symbol to resolve against the
// label table. Unlike the 8080/Z80 dialects, this one *does* support
// `name = expr` equates (`Line::Equate` below) - used only to pin
// this backend's own fixed memory cells (`ac`/`bc`/.../the `sv*`
// save-stack - see I8008Codegen.h's own header comment) to addresses
// tests/run_tests.sh's own from-scratch Python 8008 emulator can
// read directly by fixed address, without needing to parse this
// assembler's own symbol table at all - a test-harness convenience,
// not a hardware addressing restriction the way the 6502's own
// zero-page equates are (see M6502Assembler.h's own comment).
//
// Every instruction's *size* is fixed by its mnemonic alone, never by
// any operand's resolved value - same convention, and the same
// reason, as every other backend's own in-process assembler.
//
// Throws std::runtime_error on any assembly error.
std::string assembleI8008(const std::string &asmText);

// The real separate-compilation entry point (`--format=coff`/`elf`/
// `macho`/`aout` - see I8008Codegen.h's own `objectMode`) - see
// ../z80/Z80Assembler.h's own `assembleZ80Object` for the exact
// contract this mirrors (undefined symbols deferred as relocations,
// every local-address reference also relocated against the reserved
// pseudo-symbol ".." once linked). The `name = expr` equates
// (`ac`/`bc`/.../`sp` - see I8008Codegen.h's own header comment) are
// a compile-time constant in every object regardless of where it's
// linked, so - like ../m6502/M6502Assembler.h's own equates - they're
// never treated as needing a `..` relocation (see
// I8008Assembler.cpp's own `equateNames_`).
ObjectFile assembleI8008Object(const std::string &asmText);

}  // namespace pcc
