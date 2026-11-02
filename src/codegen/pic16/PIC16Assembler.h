#pragma once

#include <string>

namespace pcc {

// A from-scratch, in-process two-pass assembler for the subset of
// real PIC16 mid-range (14-bit core) assembly PIC16Codegen.cpp emits -
// not a general-purpose PIC assembler (no macros, no conditional
// assembly, no `--format=coff` - see PIC16Codegen.h's own header
// comment: this backend only supports a single, fully self-contained
// `--format=hex` compile). Uses real, standard PIC16 mnemonics and
// produces real PIC16 machine code (each 14-bit instruction word
// packed into a 16-bit little-endian cell, the exact on-disk shape
// Microchip's own Intel-HEX output uses) wrapped in a real Intel HEX
// container - the output is valid input to any accurate PIC16
// (mid-range core) simulator/programmer, not just this project's own
// in-process one.
//
// Parses via the same shared grammar ../../../grammar/PccAsm.g4 every
// other backend's own in-process assembler uses. Two private
// directives exist only for this dialect (see PIC16Codegen.h's own
// header comment on the Harvard split between code and data space):
// `orgram <addr>` sets the data-space (file-register) address cursor,
// the data-space equivalent of the ordinary `org` directive (which
// here always means the code-space word address); `res <count>`
// reserves `count` bytes of data space at the current `orgram`
// cursor and binds the line's own label to that address, advancing
// the cursor by `count`. A plain `name = value` line is a compile-
// time equate (same dialect feature, and the same M6502Assembler.cpp
// precedent, as the 6502 backend's own `ptr = 0` SFR-style bindings) -
// used here for the handful of named special-function registers
// (INDF/STATUS/FSR/INTCON) this codegen's own instructions reference
// by name.
//
// Throws std::runtime_error on any assembly error.
std::string assemblePIC16(const std::string &asmText);

}  // namespace pcc
