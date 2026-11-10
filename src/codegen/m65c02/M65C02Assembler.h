#pragma once

#include <cstdint>
#include <string>

#include "fileformat/ObjectFile.h"

namespace rcc {

// Distinct from kM6502CoffMachine (0x6502) so a 65C02 object can't link
// with an NMOS 6502 object: the CMOS-only stz/stzp opcode this backend
// emits wouldn't run on real NMOS hardware.
constexpr uint16_t kM65C02CoffMachine = 0xC502;

// Entry address for --format=raw; matches kBaseAddress in
// M65C02Codegen.cpp so a linked program behaves like a single-file one.
constexpr uint32_t kM65C02BaseAddress = 0x0600;

// Little-endian 16-bit additive patch (GenericLinker.h's RelocKinds::patchAbs16).
// The fixed zero-page pseudo-registers (ptr/ac/bc/cx/dx) are bound via
// equates to fixed compile-time constants, so references to them never
// need relocation.
constexpr uint16_t kM65C02RelocAbs16 = 1;

// One-byte additive patches for ldai/ldai_hi (an address split into separate
// low/high immediate bytes); implemented as local patch functions in
// M65C02Linker.cpp rather than added to GenericLinker.h's shared RelocKinds.
constexpr uint16_t kM65C02RelocAbsLo8 = 2;
constexpr uint16_t kM65C02RelocAbsHi8 = 3;

// Two-pass assembler for this project's WDC 65C02 dialect: identical to
// M6502Assembler.cpp's dialect plus the CMOS-only stz/stzp (STZ). Every
// operand is a plain expression; there's no register syntax, since each
// mnemonic already implies its register(s), and no bracketed memory operand
// syntax (zero-page indirect indexed uses the dedicated ldaind/staind instead).
//
// --format=raw has no relocations; an unresolved symbol is a hard error.
// --format=coff/elf/macho/aout/objraw defer unresolved symbols as
// relocations instead (see assembleM65C02Object() below).
//
// Every instruction's size is fixed by its mnemonic alone, never by an
// operand's resolved value, for a stable two-pass size computation.
//
// Throws std::runtime_error on any assembly error.
std::string assembleM65C02(const std::string &asmText);

// Object-format entry point: like assembleM65C02(), but an undefined symbol
// is deferred as a relocation instead of erroring, and every ordinary
// address reference also gets a relocation against the reserved
// pseudo-symbol ".." (this object's eventual linked base).
ObjectFile assembleM65C02Object(const std::string &asmText);

}  // namespace rcc
