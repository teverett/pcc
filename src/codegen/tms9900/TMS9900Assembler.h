#pragma once

#include <cstdint>
#include <string>

#include "fileformat/ObjectFile.h"

namespace rcc {

// No real COFF machine ID exists for the TMS9900; this is a project-local stand-in.
constexpr uint16_t kTMS9900CoffMachine = 0x9900;

// Linked base address; matches the whole-program raw entry's own `org`.
constexpr uint32_t kTMS9900BaseAddress = 0x2000;

// Reloc kinds: the TMS9900 has no 8-bit-immediate addressing at all (every reference to a
// label, in any addressing mode, is a full 16-bit word - either the "following word" an
// Indexed/Symbolic operand uses, or a Format 8 instruction's own immediate word), so this
// backend needs only one relocation kind, unlike the 6502/z80's split abs16/lo/hi set.
constexpr uint16_t kTMS9900RelocAbs16 = 1;
// Format 2's conditional jumps (Jxx/JMP) encode an 8-bit signed PC-relative word
// displacement in the opcode word itself; every one of this dialect's own jump targets is a
// same-file label fully resolved within the same two-pass run, so (like every other backend's
// own short-branch instructions) this never needs its own relocation kind - an out-of-range or
// cross-object target is a plain assembly error instead. A real long-range, relocatable branch
// instead uses Format 6's `b`/`bl`/`blwp` with a Symbolic (`[label]`) operand, going through
// `kTMS9900RelocAbs16` like any other label reference.

// Two-pass, in-process assembler for the dialect TMS9900Codegen.cpp emits (not a general-
// purpose TMS9900 assembler, though the opcode table itself is complete - see this file's own
// header comment on grammar/RccAsm.g4's own shared syntax for why addressing modes are spelled
// the way they are here).
//
// grammar/RccAsm.g4 has no `*Rn`/`@label`/`@label(Rn)`/postfix-`+` syntax (the real TMS9900
// assembly notation for register-indirect/symbolic/indexed/autoincrement addressing) - rather
// than extend that shared grammar (used by every other backend's own assembler too) just for
// this one target, every addressing mode here is instead spelled using the grammar's existing
// bracket-plus-expression shape, the same way M68kAssembler.cpp's own header comment describes
// doing for 68000's `(An)`/`d16(An)` modes:
//   r0..r15        register direct            (Rn)
//   [r0]..[r15]    register indirect           (*Rn)
//   [ir0]..[ir15]  register indirect autoinc   (*Rn+)
//   [1000]/[label] symbolic (direct memory)    (@1000 / @label)
//   [r3+1000]      indexed                     (@1000(R3))
//   a bare number/label, with no brackets at all, used only where a Format 8 instruction
//   (LI/AI/ANDI/ORI/CI/LWPI/LIMI) expects its own immediate word - the TMS9900 has no general
//   immediate addressing mode for ordinary two-operand instructions the way most CPUs do.
//
// Throws std::runtime_error on any assembly error.
std::string assembleTMS9900(const std::string &asmText);

// Separate-compilation entry point: assembles with `org 0`, defers undefined symbols as
// relocations, and emits a relocation against pseudo-symbol ".." for every local address
// reference (resolved once linked). Returns a format-neutral ObjectFile; this function never
// writes real object bytes.
ObjectFile assembleTMS9900Object(const std::string &asmText);

}  // namespace rcc
