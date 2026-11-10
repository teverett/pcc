#pragma once

#include <cstdint>
#include <string>

#include "fileformat/ObjectFile.h"

namespace pcc {

// Ad-hoc COFF machine ID for this dialect, ASCII "VA" (no real VAX COFF ID exists).
constexpr uint16_t kVaxCoffMachine = 0x5641;

// Load/entry address for a linked whole program; must match VaxCodegen.cpp's kBaseAddress.
constexpr uint32_t kVaxBaseAddress = 0x00004000u;

// The one relocation kind this target needs: a plain 32-bit little-endian address field
// (an absolute-mode operand, a symbolic immediate, or a `dw` data word), patched as an
// ordinary add. Every cross-object reference goes through absolute addressing, so no
// PC-relative kind exists.
constexpr uint16_t kVaxRelocAbs32 = 1;

// In-process two-pass assembler for the VAX-11/780 dialect VaxCodegen.cpp emits. Every
// instruction encodes real VAX machine code bit-for-bit (little-endian, variable length,
// one operand specifier byte per operand); this dialect only narrows which mnemonics exist
// and spells addressing modes so they fit the shared PccAsm grammar, which has no `#`, `@`
// or trailing `+` token:
//
//   r0..r11, ap, fp, sp   register                      (mode 5)
//   (rN)                  register deferred             (mode 6)
//   -(rN)                 autodecrement                 (mode 7)
//   ai (rN)               autoincrement                 (mode 8)
//   D(rN)                 displacement, D a plain number; byte/word/long picked by size
//   imm V                 immediate: a literal 0..63 becomes a one-byte short literal,
//                         any other number an operand-size-wide immediate, and a symbol
//                         a 32-bit relocated address
//   abs V                 absolute address              (mode 9, register 15)
//
// Branch targets are bare labels. `brb`/`brw` and the `b<cc>` conditionals take a byte or
// word displacement; the `jbr` and `j<cc>` pseudo-instructions are fixed-size and reach the
// whole word range: `jbr` is `brw`, and `j<cc>` is the inverse `b<cc>` over a `brw`, since
// real VAX conditional branches only have a byte displacement.
//
// `db`/`dh`/`dw` emit 1/2/4-byte little-endian data (a function's 2-byte CALLS entry mask
// is `dh 0`). Throws std::runtime_error on any assembly error.
std::string assembleVax(const std::string &asmText);

// The separate-compilation entry point (`--format=coff`/`elf`/`macho`/`aout`): assembles
// `asmText` (generated at `org 0`) like assembleVax(), except an undefined symbol defers as
// a relocation instead of erroring, and every symbolic address field - even to a label in
// this same file - also relocates against the reserved pseudo-symbol ".." (every label
// resolves only to "offset from org 0" until that base is added).
ObjectFile assembleVaxObject(const std::string &asmText);

}  // namespace pcc
