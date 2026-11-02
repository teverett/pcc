#pragma once

#include <cstdint>
#include <string>

#include "objectformat/ObjectFile.h"

namespace pcc {

// This target's own custom, non-registered COFF/object-format
// machine ID (no real one exists for the 8080 in this scheme - see
// ../../coff/CoffWriter.h's own header comment) - exported so
// ../../linker/I8080LinkerFactory.cpp can check an input object's
// embedded machine type against `--target=8080` without duplicating
// this literal.
constexpr uint16_t kI8080CoffMachine = 0x8080;

// Where the whole program loads and starts running once real
// separate compilation (`--format=coff`/`elf`/`macho`/`aout`, see
// `allowExternal` below) and `--link` are both involved - the same
// address I8080Codegen.cpp's own whole-program `--format=raw` entry
// sequence already uses (`kBaseAddress`), kept identical so a linked
// program behaves exactly like a single-file one. Exported so
// I8080LinkerFactory.cpp's own sequential object layout starts from
// the same place.
constexpr uint32_t kI8080BaseAddress = 0x0100;

// This dialect's one relocation kind: a little-endian 16-bit
// *additive* patch (see ../../linker/GenericLinker.h's own
// `RelocKinds::patchAbs16`) - every 8080 jump/call/16-bit-load this
// backend ever emits is a full absolute address (see I8080Codegen.h's
// own header comment: "every real 8080 jump/call is an unconditional-
// range 16-bit absolute address"), so this is the only relocation
// kind the dialect needs at all.
constexpr uint16_t kI8080RelocAbs16 = 1;

// A from-scratch, in-process two-pass assembler for the subset of
// real Intel 8080 assembly I8080Codegen.cpp emits - not a general-
// purpose 8080 assembler (no macros, no conditional assembly - see
// I8080Codegen.h's own header comment). Unlike the
// 6502/68000 backends' own in-process assemblers (which invent a
// narrow private mnemonic dialect - see M6502Assembler.h), this one
// uses real, standard 8080 mnemonics and produces real 8080 machine
// code: every opcode byte emitted here matches Intel's own published
// encoding, so the output is valid input to any accurate 8080 (or
// object-code-compatible Z80) emulator or disassembler, not just this
// project's own in-process one.
//
// Parses via the same shared grammar ../../../grammar/PccAsm.g4 every
// other backend's own in-process assembler uses; a register name
// (`a`, `b`, ..., `h`, `l`, `m`, `sp`, `psw`) is syntactically just
// another bare identifier to that grammar - this file is what decides,
// per mnemonic and operand position, whether a given identifier names
// a register or a symbol to resolve against the label table.
//
// Every instruction's *size* is fixed by its mnemonic alone, never by
// any operand's resolved value - same convention, and the same
// reason, as every other backend's own in-process assembler.
//
// Throws std::runtime_error on any assembly error.
std::string assembleI8080(const std::string &asmText);

// The real separate-compilation entry point (`--format=coff`/`elf`/
// `macho`/`aout` - see I8080Codegen.h's own `objectMode`): assembles
// `asmText` (generated with `org 0`, not `kI8080BaseAddress` - see
// I8080Codegen.cpp's own `generate()`) exactly like assembleI8080()
// does, except an undefined symbol is deferred as a relocation (see
// ../../objectformat/ObjectFile.h's own `ObjRelocation`) instead of a
// hard error, and *every* address reference - even to a label defined
// in this same file - also gets an additional relocation against the
// reserved pseudo-symbol ".." (this object's own eventual linked
// base, resolved only by a linker - see
// ../../linker/GenericLinker.h's own header comment on why). Returns
// the format-neutral ObjectFile a codegen backend builds once per
// compile (see ObjectFile.h) - this function never touches a real
// COFF/ELF/Mach-O/a.out byte layout itself; that happens only in
// whichever ObjectFormatWriter the caller picks for the requested
// `--format` (see ../../objectformat/ObjectFormat.h).
ObjectFile assembleI8080Object(const std::string &asmText);

}  // namespace pcc
