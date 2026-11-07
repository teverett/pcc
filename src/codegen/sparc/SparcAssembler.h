#pragma once

#include <cstdint>
#include <string>

#include "fileformat/ObjectFile.h"

namespace pcc {

// Ad-hoc COFF machine ID for this dialect, ASCII "SP" (no real SPARC COFF ID exists).
constexpr uint16_t kSparcCoffMachine = 0x5350;

// Load/entry address for a linked whole program; must match SparcCodegen.cpp's kBaseAddress.
constexpr uint32_t kSparcBaseAddress = 0x00004000u;

// Local relocation kinds: sethi/or's split address and call's 30-bit PC-relative field
// aren't plain additive patches, so they aren't shared via GenericLinker.h's RelocKinds.
//
// kSparcRelocHi/kSparcRelocLo: upper 22 / lower 10 bits of an address, split across a
// `sethi %hi(addr),rd` / `or rd,%lo(addr),rd` pair always emitted 4 bytes apart so the
// linker's Hi patch can find its paired Lo site at +4. The linker's own base/symbol
// correction, added to each field separately, can carry from the 10-bit Lo field into
// the 22-bit Hi field.
constexpr uint16_t kSparcRelocHi = 1;
constexpr uint16_t kSparcRelocLo = 2;
// kSparcRelocCall: call's 30-bit word-aligned PC-relative field. A call to a label in
// this same object needs no relocation; only a cross-object call target does.
constexpr uint16_t kSparcRelocCall = 3;
// kSparcRelocAbs32: a plain 32-bit data word (a `dw` line), patched as an ordinary add.
constexpr uint16_t kSparcRelocAbs32 = 4;

// In-process two-pass assembler for the narrow 32-bit SPARC V8 dialect SparcCodegen.cpp
// emits. Every operand is a plain expression (number, register name, or label), not real
// `[%rs1+offset]`/`%hi()`/`%lo()` syntax: addressing modes that don't fit a bare operand
// get dedicated mnemonics (`ldind`/`stind`), and `sethi`/`or` resolve a label operand to
// its own upper-22/lower-10 bits automatically instead of needing `%hi`/`%lo`.
//
// Every instruction encodes real SPARC V8 machine code bit-for-bit; this dialect only
// narrows which mnemonics/operand shapes exist. Every instruction is 4 bytes, except
// `call`, `ba`, the six `b<cc>` branches, and `retl`, which are 8 bytes: the real
// branch/call word followed by a synthesized `nop` delay-slot instruction.
//
// Throws std::runtime_error on any assembly error.
std::string assembleSparc(const std::string &asmText);

// The separate-compilation entry point (`--format=coff`/`elf`/`macho`/`aout`): assembles
// `asmText` (generated at `org 0`) like assembleSparc(), except an undefined symbol
// defers as a relocation instead of erroring, and every `sethi`/`or` address reference -
// even to a label in this same file - also relocates against the reserved pseudo-symbol
// ".." (every label resolves only to "offset from org 0" until that base is added). A
// `call` to a label in this file needs no such relocation. Returns the format-neutral
// ObjectFile a codegen backend builds once per compile.
ObjectFile assembleSparcObject(const std::string &asmText);

}  // namespace pcc
