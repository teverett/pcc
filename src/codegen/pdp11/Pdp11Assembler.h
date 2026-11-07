#pragma once

#include <cstdint>
#include <string>

#include "fileformat/ObjectFile.h"

namespace pcc {

// This target's own custom COFF/object-format machine ID (no real one exists for this dialect).
constexpr uint16_t kPdp11CoffMachine = 0x1121;

// Load/entry address for a linked whole program; matches Pdp11Codegen.cpp's raw-format kBaseAddress.
constexpr uint32_t kPdp11BaseAddress = 0x1000;

// `kPdp11RelocAbs16`: little-endian 16-bit additive patch, used only by `lea label,Rn`'s
// PC-immediate extension word (the one place a label is treated as an absolute address).
constexpr uint16_t kPdp11RelocAbs16 = 1;
// `kPdp11RelocRel16`: PC-relative extension-word formula `value - siteFinalAddr`, with no +2
// adjustment (unlike GenericLinker.h's patchRel16). Used by every other bare-label operand,
// including `jsr label`. A same-object reference never needs this: target and extension-word
// address are already final zero-based offsets, so their difference needs no base-shift.
constexpr uint16_t kPdp11RelocRel16 = 2;

// A from-scratch, in-process two-pass assembler for exactly the narrow PDP-11 dialect
// Pdp11Codegen.cpp emits - not a general-purpose MACRO-11 assembler. Parses via the shared
// grammar/PccAsm.g4 grammar. There's no real MACRO-11 `(Rn)`/`#n`/`@#n` syntax: the grammar
// can't distinguish "(expr)" from "(Rn)", so each addressing mode that can't be a bare
// register-or-label operand gets its own mnemonic (`ldind`/`stind` for `(Rn)`, `push`/`pop`
// for `-(SP)`/`(SP)+`, `lea` for a label's address). The one exception is the grammar's
// `[expr]` bracket syntax, read here as index mode `X(Rn)` (`[Rn+N]`/`[Rn-N]`), used for
// frame-pointer-relative Local/Param addressing.
//
// A bare label operand (other than `lea`'s source) is PDP-11 relative addressing (mode 067,
// PC as base), reading/writing the value stored there; `lea`'s source instead means "the
// label's own address", via PC-relative immediate addressing (mode 027).
//
// This project's 16-bit `int` matches the PDP-11 word size, so no `.B`-suffixed byte op is
// ever emitted.
//
// Throws std::runtime_error on any assembly error.
std::string assemblePdp11(const std::string &asmText);

// The separate-compilation entry point: assembles `asmText` (generated with `org 0`) like
// assemblePdp11(), except an undefined symbol is deferred as a relocation instead of a hard
// error. Returns the format-neutral ObjectFile; never touches a real object-format byte layout.
ObjectFile assemblePdp11Object(const std::string &asmText);

}  // namespace pcc
