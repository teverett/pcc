#pragma once

#include <cstdint>
#include <string>

#include "fileformat/ObjectFile.h"

namespace pcc {

// This dialect's own machine ID; not a registered COFF value.
constexpr uint16_t kMipsCoffMachine = 0x4000;

// Must match MipsCodegen.cpp's own kBaseAddress so linked and
// single-file programs load at the same address.
constexpr uint32_t kMipsBaseAddress = 0x80010000u;

// Local relocation kinds, not GenericLinker.h's RelocKinds: LUI/ORI
// address loads and J/JAL's 26-bit field aren't plain additive patches.
// Hi/Lo: upper/lower 16 bits of an address, split across an adjacent
// LUI/ORI pair (always 4 bytes apart, so the linker's Hi patch can
// find the paired Lo site at +4 for carry-correct 32-bit patching).
constexpr uint16_t kMipsRelocHi = 1;
constexpr uint16_t kMipsRelocLo = 2;
// J/JAL's 26-bit instr_index field: (address & 0x0FFFFFFF) >> 2.
constexpr uint16_t kMipsRelocJump = 3;
// A plain 32-bit data word (`dw`); ordinary additive patch.
constexpr uint16_t kMipsRelocAbs32 = 4;

// Two-pass assembler for exactly the narrow MIPS dialect MipsCodegen.cpp
// emits, not a general-purpose MIPS assembler. The grammar has no real
// `0(reg)`/`%hi()`/`%lo()` syntax, so memory access uses dedicated
// `ldind`/`stind` mnemonics instead, and LUI/ORI resolve a label operand
// to its upper/lower 16 bits automatically. Emits only MIPS I
// instructions, each a fixed 4 bytes; branch-delay-slot NOPs are
// MipsCodegen.cpp's responsibility, not this assembler's.
// Throws std::runtime_error on any assembly error.
std::string assembleMips(const std::string &asmText);

// Separate-compilation entry point: assembles `asmText` (emitted with
// `org 0`) like assembleMips(), but defers undefined symbols as
// relocations instead of failing, and additionally relocates every
// local address reference against the reserved pseudo-symbol ".."
// (see GenericLinker.h) since the object has no fixed base address yet.
ObjectFile assembleMipsObject(const std::string &asmText);

}  // namespace pcc
