#pragma once

#include <cstdint>
#include <string>

#include "fileformat/ObjectFile.h"

namespace pcc {

// Custom, non-registered COFF machine ID. Distinct from mc68328's despite sharing
// the 68000 instruction set, since the two use incompatible entry sequences
// (direct PC set vs. reset-vector table) and must not be linked together.
constexpr uint16_t kM68kCoffMachine = 0x6800;

// Load address for linked programs; matches the raw-format entry address so
// linked and single-file output behave identically.
constexpr uint32_t kM68kBaseAddress = 0x1000;

// The only relocation kind needed: a big-endian 32-bit absolute patch (68000 is
// big-endian). Branches never cross files, so no relative-branch kind is needed.
constexpr uint16_t kM68kRelocAbs32 = 1;

// Two-pass assembler for the narrow 68000 dialect M68kCodegen.cpp emits, not a
// general-purpose 68000 assembler. The grammar has no `#imm`/`(An)` syntax, so
// addressing modes other than the bracketed "[aN+disp]" frame-relative form get
// their own dedicated mnemonics instead (`ldind`/`stind` for (An), `pushl`/`popl`
// for -(A7)/(A7)+, `linkl`/`unlkl` for LINK/UNLK). The bracketed form is always
// relative to a6, the frame pointer. Only 32-bit (.L) forms are implemented, plus
// 16-bit MULS.W/DIVS.W since multiply/divide are 16-bit-only on this CPU.
// Throws std::runtime_error on any assembly error.
std::string assembleM68k(const std::string &asmText);

// Separate-compilation entry point: assembles `asmText` (emitted with org 0) like
// assembleM68k(), but defers undefined symbols as relocations instead of erroring,
// and additionally relocates every absolute reference, even to a same-file label,
// against the reserved pseudo-symbol ".." since labels resolve to an offset from
// org 0 until the link-time base is added.
ObjectFile assembleM68kObject(const std::string &asmText);

}  // namespace pcc
