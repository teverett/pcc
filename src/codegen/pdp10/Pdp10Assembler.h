#pragma once

#include <cstdint>
#include <string>

#include "fileformat/ObjectFile.h"

namespace rcc {

// This target's own custom COFF/object-format machine ID (no real one exists for this dialect).
constexpr uint16_t kPdp10CoffMachine = 0x1010;

// Load/entry address for a linked whole program; matches Pdp10Codegen.cpp's raw-format
// kBaseAddress. Octal 1000 (512 decimal) - comfortably past the low-memory region real PDP-10
// software reserves for the exec's own trap/interrupt vectors.
constexpr uint32_t kPdp10BaseAddress = 0010000;

// The only relocation kind this dialect needs: every address this backend ever writes is a
// plain 18-bit direct address occupying a word's low 18 bits (no PC-relative addressing mode
// is ever emitted - see Pdp10Codegen.cpp's own header comment), so a reference to any label
// (same-object or external) is always an additive patch into exactly that field, clearing it
// first so a repeated patch (object's own base shift, then an external symbol) composes
// correctly.
constexpr uint16_t kPdp10RelocAbs18 = 1;

// A from-scratch, in-process two-pass assembler for exactly the narrow PDP-10 dialect
// Pdp10Codegen.cpp emits - not a general-purpose MACRO-10 assembler. Parses via the shared
// grammar/RccAsm.g4 grammar.
//
// Every instruction here has the same uniform shape "mnemonic ac,E": a 4-bit accumulator
// field (`ac`, always a bare literal 0-17 - this dialect never needs a symbolic register name,
// since Pdp10Codegen.cpp always emits the plain decimal AC number) and one general operand
// field `E` (1 indirect bit - never set by this dialect - plus a 4-bit index register and an
// 18-bit address/immediate `y`, matching the real PDP-10 instruction word format exactly:
// opcode(9) | ac(4) | i(1) | x(4) | y(18) = 36 bits). Which of "C(E)" (dereferenced) or "E
// itself" (immediate) an opcode's `E` means is pure CPU runtime semantics, invisible to this
// assembler - Pdp10Codegen.cpp picks the already-correct opcode (e.g. `movei` vs `move`) for
// what it wants.
//
// `E`'s text is either a bare expression (a literal number, or a label - a label is always an
// absolute address, so it's relocated the same way a `dw` data word referencing a label is) or
// this dialect's one special form, index mode: `[ACn+N]`/`[ACn-N]` (the shared grammar's `[...]`
// bracket syntax, reused here exactly as Pdp11Assembler.h reuses it for frame-pointer-relative
// addressing - see its own header comment), giving index register `ACn` and displacement `N`
// (always a compile-time literal here, never a relocated symbol).
//
// `org`/`dw` behave like Pdp11Assembler's own: `org` sets the relocation base, `dw` declares
// one 36-bit word of data per operand (literal or symbol, each independently relocatable the
// same way an instruction's `E` is).
//
// Throws std::runtime_error on any assembly error.
std::string assemblePdp10(const std::string &asmText);

// The separate-compilation entry point: assembles `asmText` (generated with `org 0`) like
// assemblePdp10(), except an undefined symbol is deferred as a relocation instead of a hard
// error. Returns the format-neutral ObjectFile; never touches a real object-format byte layout.
ObjectFile assemblePdp10Object(const std::string &asmText);

}  // namespace rcc
