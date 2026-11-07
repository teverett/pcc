#pragma once

#include <cstdint>
#include <string>

#include "fileformat/ObjectFile.h"

namespace pcc {

// Non-standard COFF machine ID for this dialect (ASCII "PC"), checked by
// PowerPCLinkerFactory.cpp against `--target=powerpc`.
constexpr uint16_t kPowerPCCoffMachine = 0x5043;

// Load/entry address for a linked whole program; matches PowerPCCodegen.cpp's
// own raw-mode entry address so a linked program behaves like a single-file
// one.
constexpr uint32_t kPowerPCBaseAddress = 0x00100000u;

// Local relocation kinds, not shared via GenericLinker::RelocKinds, because
// lis/ori's split address load and b/bl's branch field aren't plain additive
// patches.
//
// Upper/lower 16 bits of an address, split across a lis/ori pair (always
// emitted lis immediately followed by ori, 4 bytes apart, so the linker's Hi
// patch can find the paired Lo site at a fixed +4 offset).
constexpr uint16_t kPowerPCRelocHi = 1;
constexpr uint16_t kPowerPCRelocLo = 2;
// b/bl's 24-bit branch displacement is PC-relative to its own address, so a
// same-object branch target needs no relocation; only a genuinely
// cross-object bl target does.
constexpr uint16_t kPowerPCRelocBranch = 3;
// A plain additive 32-bit data word (a `dw` line's own word).
constexpr uint16_t kPowerPCRelocAbs32 = 4;

// Two-pass, in-process assembler for this project's own narrow PowerPC
// dialect (not a general-purpose PowerPC assembler), parsed via the shared
// grammar/PccAsm.g4. Every operand is a bare register, number, or label -
// there is no `0(r4)`/`%hi()`/`%lo()` syntax; an addressing mode that needs
// one instead gets its own dedicated mnemonic (`ldind`/`stind`, fixed zero
// displacement), and `lis`/`ori` resolve a label operand to its address's
// upper/lower 16 bits automatically.
//
// Every encoding here matches genuine 32-bit classic PowerPC machine code
// bit-for-bit; this dialect only narrows which mnemonics and operand shapes
// exist, never what a given real encoding means.
//
// Every instruction is a fixed 4 bytes; PowerPC has no branch-delay slot.
// Throws std::runtime_error on any assembly error.
std::string assemblePowerPC(const std::string &asmText);

// The separate-compilation entry point (`--format=coff`/`elf`/`macho`/`aout`
// - see PowerPCCodegen.h's `objectMode`): like assemblePowerPC(), except an
// undefined symbol is deferred as a relocation instead of a hard error, and
// every `lis`/`ori` address reference - even to a label defined in this same
// file - also gets a relocation against the pseudo-symbol ".." (this
// object's own base), since this text's labels all resolve to an org-0-
// relative offset until linked. A `b`/`bl` branch to a same-file label needs
// no such relocation; only a genuinely external `bl` target does.
ObjectFile assemblePowerPCObject(const std::string &asmText);

}  // namespace pcc
