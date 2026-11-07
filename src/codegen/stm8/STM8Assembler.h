#pragma once

#include <cstdint>
#include <string>

#include "fileformat/ObjectFile.h"

namespace pcc {

// Custom, unregistered COFF machine ID for STM8 (no real one exists).
constexpr uint16_t kSTM8CoffMachine = 0x5708;

// Load/entry address for a linked whole program; matches STM8Codegen.cpp's
// `--format=raw` entry point so a linked program behaves like a single-file one.
constexpr uint32_t kSTM8BaseAddress = 0x0100;

// This dialect's one relocation kind: a big-endian 16-bit additive patch
// (see GenericLinker.h's RelocKinds::patchAbs16BE).
constexpr uint16_t kSTM8RelocAbs16 = 1;

// In-process two-pass assembler for this project's own narrow STM8 dialect
// (not a general-purpose STM8 assembler); the encoding is this project's
// own invention, not guaranteed to match a real STM8 toolchain byte-for-byte.
// Parses via the shared grammar/PccAsm.g4; a register name (a/x/y/sp) is
// just a bare identifier there, so this file decides per mnemonic/operand
// position whether an identifier is a register or a label reference.
// Two deliberate departures from real STM8 hardware:
//   - conditional jumps (jreq/jrne/jrult/jruge) are full 16-bit absolute,
//     not the real 8-bit signed relative JRxx.
//   - indirect-through-x addressing (real LD A,(X)/LD (X),A) uses the
//     dedicated mnemonics ldxind/stxind instead of a (x) operand, since
//     the grammar's '(' already means a parenthesized sub-expression.
// Every instruction's size is fixed by its mnemonic alone.
// Throws std::runtime_error on any assembly error.
std::string assembleSTM8(const std::string &asmText);

// The separate-compilation entry point: assembles like assembleSTM8(),
// except an undefined symbol becomes a relocation instead of a hard error,
// and every address reference also gets a relocation against the reserved
// pseudo-symbol ".." (this object's own eventual linked base, added only
// by the linker).
ObjectFile assembleSTM8Object(const std::string &asmText);

}  // namespace pcc
