#pragma once

#include <cstdint>
#include <string>

#include "fileformat/ObjectFile.h"

namespace rcc {

// Custom COFF machine ID for this dialect; low byte names ARMv7-M/ARMv7E-M,
// shared by the cortex-m3/m4/m7 target aliases since they emit identical code.
constexpr uint16_t kArmMCoffMachine = 0xA007;

// Load address for a linked whole program; matches kBaseAddress used by the
// --format=raw entry sequence so linked and single-file programs behave alike.
constexpr uint32_t kArmMBaseAddress = 0x1000u;

// Relocation kinds, only fired when a branch/address target is undefined locally:
//   MovWLo/MovTHi - low/high 16 bits of an address, split across MOVW/MOVT's
//                   non-contiguous imm4:i:imm3:imm8 fields.
//   Branch        - BL/wide-B's PC-relative field, scrambled via Thumb-2's
//                   S:I1:I2:imm10:imm11 (J1/J2) encoding.
constexpr uint16_t kArmMRelocMovWLo = 1;
constexpr uint16_t kArmMRelocMovTHi = 2;
constexpr uint16_t kArmMRelocBranch = 3;

// From-scratch two-pass assembler for the narrow ARM Thumb dialect
// ArmMCodegen.cpp emits. Operands are plain expressions (number, register,
// or label), not real ARM syntax: no bracketed memory operand or {reglist},
// so addressing/multi-register forms get dedicated mnemonics instead -
// ldind/stind (fixed zero displacement), push/pop (one register), and
// pushlr/poppc for the fixed prologue/epilogue register sets.
// movw/movt resolve a label operand to its lower/upper 16 bits respectively
// (MOVW carries the low half, MOVT the high).
// Only emits real ARMv7-M Thumb/Thumb-2; each conditional branch mnemonic
// expands to an inverted-condition Bcc followed by a wide unconditional B
// (see encodeConditionalBranch()).
// Every mnemonic has a fixed size (2 bytes Thumb-1, 4 bytes wide Thumb-2,
// 6 bytes for the two-instruction conditional branches).
// Throws std::runtime_error on any assembly error.
std::string assembleArmM(const std::string &asmText);

// Separate-compilation entry point: like assembleArmM(), but an undefined
// symbol becomes a relocation instead of a hard error. Every MOVW/MOVT
// address reference also gets a relocation against the reserved pseudo-
// symbol ".." to add the object's load-base offset, since labels resolve
// only to "offset from org 0" until linked. A BL/wide-B branch is already a
// correct PC-relative reference regardless of load base, so it gets a
// relocation only when its target is undefined in this file.
ObjectFile assembleArmMObject(const std::string &asmText);

}  // namespace rcc
