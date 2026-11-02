#pragma once

#include <cstdint>
#include <string>

#include "fileformat/ObjectFile.h"

namespace pcc {

// This target's own custom, non-registered COFF/object-format
// machine ID (no real one exists for the STM8 - see
// ../../fileformat/coff/CoffFormat.h's own header comment) - exported so
// ../../linker/STM8LinkerFactory.cpp can check an input object's
// embedded machine type against `--target=stm8` without duplicating
// this literal.
constexpr uint16_t kSTM8CoffMachine = 0x5708;

// Where the whole program loads and starts running once real
// separate compilation (`--format=coff`/`elf`/`macho`/`aout`, see
// `allowExternal` below) and `--link` are both involved - the same
// address STM8Codegen.cpp's own whole-program `--format=raw` entry
// sequence already uses (`kBaseAddress`), kept identical so a linked
// program behaves exactly like a single-file one. Exported so
// STM8LinkerFactory.cpp's own sequential object layout starts from
// the same place.
constexpr uint32_t kSTM8BaseAddress = 0x0100;

// This dialect's one relocation kind: a big-endian 16-bit *additive*
// patch (see ../../linker/GenericLinker.h's own `RelocKinds::
// patchAbs16BE`) - every address this backend's own codegen ever
// emits (`ldwi`'s own 16-bit immediate, and every jump/call target,
// including the conditional ones - see STM8Codegen.h's own header
// comment on why those are full 16-bit absolute here, not a real
// STM8's 8-bit relative `JRxx`) goes through this dialect's one
// `emit16`, so there's only ever this one relocation kind to define,
// the same situation z80's own dialect is in.
constexpr uint16_t kSTM8RelocAbs16 = 1;

// A from-scratch, in-process two-pass assembler for exactly the
// narrow STM8 dialect STM8Codegen.cpp emits - not a general-purpose
// STM8 assembler. Parses via the same shared grammar
// ../../../grammar/PccAsm.g4 every other backend's own in-process
// assembler uses; a register name (`a`, `x`, `y`, `sp`) is
// syntactically just another bare identifier to that grammar - this
// file is what decides, per mnemonic and operand position, whether a
// given identifier names a register or a symbol to resolve against
// the label table (same convention as I8080Assembler.h).
//
// Every mnemonic here is a real STM8 one, but the *encoding* is this
// project's own, not guaranteed byte-identical to a real STM8
// toolchain's output - unlike the 6502/8080 backends, there's no
// available from-scratch STM8 emulator on PyPI to cross-check a real
// opcode encoding against (see STM8Codegen.h's own header comment),
// so correctness here means "self-consistent with what STM8Codegen.cpp
// emits", not "matches ST's own assembler byte-for-byte". Two
// deliberate departures from the real chip for exactly that reason:
//
//   - every conditional jump (`jreq`/`jrne`/`jrult`/`jruge`) is full
//     16-bit absolute, not the real STM8's 8-bit signed relative
//     `JRxx` - see STM8Codegen.h.
//   - indirect-through-`x` addressing (real `LD A,(X)`/`LD (X),A`)
//     has its own dedicated zero-operand mnemonics, `ldxind`/
//     `stxind`, instead of a literal `(x)` operand - the shared
//     grammar's `(` already means "parenthesized sub-expression" (see
//     Pdp11Assembler.h's own comment on the exact same problem with
//     `(Rn)`), so there's no way to tell "indirect through register x"
//     apart from "the symbol x, grouped" without a dedicated mnemonic.
//
// Every instruction's *size* is fixed by its mnemonic alone, never by
// any operand's resolved value - same convention, and the same
// reason, as every other backend's own in-process assembler.
//
// Throws std::runtime_error on any assembly error.
std::string assembleSTM8(const std::string &asmText);

// The real separate-compilation entry point (`--format=coff`/`elf`/
// `macho`/`aout` - see STM8Codegen.h's own `objectMode`): assembles
// `asmText` (generated with `org 0`, not `kSTM8BaseAddress` - see
// STM8Codegen.cpp's own `generate()`) exactly like assembleSTM8()
// does, except an undefined symbol is deferred as a relocation (see
// ../../fileformat/ObjectFile.h's own `ObjRelocation`) instead of a
// hard error, and *every* address reference - even to a label defined
// in this same file - also gets an additional relocation against the
// reserved pseudo-symbol ".." (this object's own eventual linked
// base, resolved only by a linker - see
// ../../linker/GenericLinker.h's own header comment on why: this
// text's own labels all resolve to "offset from this object's own
// org 0" until that base is added). Returns the format-neutral
// ObjectFile a codegen backend builds once per compile (see
// ObjectFile.h) - this function never touches a real COFF/ELF/
// Mach-O/a.out byte layout itself; that happens only in whichever
// FileFormatWriter the caller picks for the requested `--format`
// (see ../../fileformat/FileFormat.h).
ObjectFile assembleSTM8Object(const std::string &asmText);

}  // namespace pcc
