#pragma once

#include <cstdint>
#include <string>

#include "objectformat/ObjectFile.h"

namespace pcc {

// This dialect's own custom, non-registered COFF/object-format
// machine ID (no real one exists for this project's own MIPS dialect
// - see ../../coff/CoffWriter.h's own header comment) - exported so
// ../../linker/factory/MipsLinkerFactory.cpp can check an input
// object's embedded machine type against `--target=r10000`/`r4000`
// without duplicating this literal.
constexpr uint16_t kMipsCoffMachine = 0x4000;

// Where the whole program loads and starts running once real
// separate compilation (`--format=coff`/`elf`/`macho`/`aout`, see
// `allowExternal` below) and `--link` are both involved - the same
// address MipsCodegen.cpp's own (private) whole-program `kBaseAddress`
// already uses, kept identical (duplicated here, same convention as
// M68kAssembler.h's own `kM68kBaseAddress`) so a linked program
// behaves exactly like a single-file one, and so MipsLinkerFactory.cpp's
// own sequential object layout starts from the same place.
constexpr uint32_t kMipsBaseAddress = 0x80010000u;

// This dialect's three relocation kinds - all local-to-this-target,
// not shared via GenericLinker.h's own RelocKinds (see
// MipsLinkerFactory.cpp's own patch functions for why: MIPS's split
// LUI/ORI address load and its own 26-bit pseudo-absolute jump/call
// encoding aren't a plain contiguous-field additive patch the way
// every other migrated backend's own relocation kind is).
//
// `kMipsRelocHi`/`kMipsRelocLo`: the upper/lower 16 bits of a full
// 32-bit address, as split across a `LUI rt,<hi>` / `ORI rt,rt,<lo>`
// pair (see MipsAssembler.cpp's own `loadAddress`-adjacent `ori`/`lui`
// encoding) - always emitted as LUI immediately followed by ORI (4
// bytes apart), so MipsLinkerFactory.cpp's own Hi patch function can
// find its paired Lo site at a fixed `+4` offset to do carry-correct
// 32-bit patching (same carry issue, and the same fix, as the i8008
// migration's own `patchAbs16Hi`/`patchAbs16Lo` pair).
constexpr uint16_t kMipsRelocHi = 1;
constexpr uint16_t kMipsRelocLo = 2;
// `kMipsRelocJump`: J/JAL's own 26-bit pseudo-absolute instruction-
// index field (`instr_index = (address & 0x0FFFFFFF) >> 2`) - see
// MipsAssembler.cpp's own `emitJ`.
constexpr uint16_t kMipsRelocJump = 3;

// A from-scratch, in-process two-pass assembler for exactly the
// narrow MIPS dialect MipsCodegen.cpp emits - not a general-purpose
// MIPS assembler. Parses via the same shared grammar
// ../../../grammar/PccAsm.g4 every other backend's own in-process
// assembler uses (see M68kAssembler.h's own header comment). Every
// operand is a plain expression (a number, a register name, or a
// label) classified by MipsCodegen.cpp's own convention, not real
// `0(reg)`/`%hi()`/`%lo()` MIPS syntax - the grammar has no way to
// express a displacement-from-register memory operand or a function-
// style `%hi`/`%lo` modifier, so every addressing mode this dialect
// can't express as a bare register-or-label operand instead gets its
// own dedicated mnemonic (`ldind`/`stind`, always a fixed zero
// displacement - see MipsCodegen.h's own header comment on why every
// load/store here is always through a freshly computed address), and
// `LUI`/`ORI` resolve a *label* operand (as opposed to a plain
// number) to that label's own upper/lower 16 bits automatically
// instead of needing `%hi`/`%lo` spelled out at all.
//
// Only 32-bit MIPS I instructions are emitted (this project's own
// `int`, chosen to match this target's native register width, same
// reasoning as every other backend's own word-size choice - see
// MipsCodegen.h) - every one of them already part of the MIPS I
// instruction set, so it runs unchanged on a real R10000 (MIPS IV).
//
// Every instruction here is a fixed 4 bytes, same convention (and
// the same reason) as every other backend's own assembler. Branch-
// delay slots are MipsCodegen.cpp's own concern (it always emits an
// explicit `NOP` right after any branch/jump - see its own header
// comment); this assembler just encodes whatever instruction stream
// it's given, delay slot or not.
//
// Throws std::runtime_error on any assembly error.
std::string assembleMips(const std::string &asmText);

// The real separate-compilation entry point (`--format=coff`/`elf`/
// `macho`/`aout` - see MipsCodegen.h's own `objectMode`): assembles
// `asmText` (generated with `org 0`, not `kMipsBaseAddress` - see
// MipsCodegen.cpp's own `generate()`) exactly like assembleMips()
// does, except an undefined symbol is deferred as a relocation
// instead of a hard error, and *every* local address reference (even
// to a label defined in this same file) also gets an additional
// relocation against the reserved pseudo-symbol ".." (see
// ../../linker/GenericLinker.h's own header comment on why). Returns
// the format-neutral ObjectFile a codegen backend builds once per
// compile (see ../../objectformat/ObjectFile.h).
ObjectFile assembleMipsObject(const std::string &asmText);

}  // namespace pcc
