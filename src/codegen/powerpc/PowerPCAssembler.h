#pragma once

#include <cstdint>
#include <string>

#include "fileformat/ObjectFile.h"

namespace pcc {

// This dialect's own custom, non-registered COFF/object-format
// machine ID (no real one exists for this project's own PowerPC
// dialect - see ../../fileformat/coff/CoffFormat.h's own header comment) -
// exported so ../../linker/factory/PowerPCLinkerFactory.cpp can check
// an input object's embedded machine type against `--target=powerpc`
// without duplicating this literal. ASCII "PC", the same ad-hoc
// "recognizably derived from this target's own name" convention as
// z80's own 0x5A38 ("Z8") and m65c02's own 0xC502 ("C502").
constexpr uint16_t kPowerPCCoffMachine = 0x5043;

// Where the whole program loads and starts running once real
// separate compilation (`--format=coff`/`elf`/`macho`/`aout`, see
// `allowExternal` below) and `--link` are both involved - the same
// address PowerPCCodegen.cpp's own (private) whole-program
// `--format=raw` entry sequence already uses (`kBaseAddress`), kept
// identical (duplicated here, same convention as M68kAssembler.h's
// own `kM68kBaseAddress`) so a linked program behaves exactly like a
// single-file one, and so PowerPCLinkerFactory.cpp's own sequential
// object layout starts from the same place.
constexpr uint32_t kPowerPCBaseAddress = 0x00100000u;

// This dialect's three relocation kinds - all local-to-this-target,
// not shared via ../../linker/GenericLinker.h's own `RelocKinds` (see
// PowerPCLinkerFactory.cpp's own patch functions for why: `lis`/`ori`'s
// split address load and `b`/`bl`'s own 24-bit branch-displacement
// field aren't a plain contiguous-field additive patch the way every
// other migrated backend's own relocation kind is).
//
// `kPowerPCRelocHi`/`kPowerPCRelocLo`: the upper/lower 16 bits of a
// full 32-bit address, as split across a `lis rX,<hi>` / `ori
// rX,rX,<lo>` pair (see PowerPCAssembler.cpp's own `evalHiLo()`) -
// always emitted as `lis` immediately followed by `ori` (4 bytes
// apart), so PowerPCLinkerFactory.cpp's own Hi patch function can find
// its paired Lo site at a fixed `+4` offset to do carry-correct 32-bit
// patching. `ori` combines the two halves with a bitwise OR, not an
// add - unlike real-world PowerPC's own `addis`/`addi` idiom, this
// dialect's `lis` always zeroes the low 16 bits itself, so there's no
// classic `@ha`/`@l` sign-extension correction to worry about at the
// *instruction* level. The carry this pair's own relocation patching
// still has to get right lives one level up, at *link* time: each
// field already holds this object's own org-0-relative half (baked in
// by this same two-pass assembler, the same way every other split-
// hi/lo target's own object-mode assembler does it), and the linker's
// correction (the object's real base, for `..`, or an external
// symbol's own full address) still has to be added to each 16-bit half
// separately, which can carry out of the low half into the high half -
// see the i8008 migration's own `patchAbs16Hi`/`patchAbs16Lo` pair for
// the identical carry issue and the identical fix (recompute the carry
// by reading the still-unpatched sibling site, and patch in the order
// that makes that sibling still be unpatched when it's read).
constexpr uint16_t kPowerPCRelocHi = 1;
constexpr uint16_t kPowerPCRelocLo = 2;
// `kPowerPCRelocBranch`: `b`/`bl`'s own 24-bit word-aligned, PC-
// relative branch-displacement field (see PowerPCAssembler.cpp's own
// `emitBranch()`) - a real PowerPC B-form branch is always relative to
// its *own* address (`NIA = CIA + EXTS(LI||0b00)`, no "+8" pipeline
// quirk the way classic ARM A32 has), so a branch to a label defined
// in *this same object* is already a valid displacement regardless of
// where this object ends up linked (both the branch site's own final
// address and its target's shift by the exact same amount - see
// PowerPCLinkerFactory.cpp's own `patchPowerPCBranch24()` comment) and
// never needs a relocation at all; only a genuinely cross-object
// `bl` - calling a function defined in some *other* input file -
// needs deferring, the same "only a truly external PC-relative
// reference needs a relocation" reasoning as Arm64Assembler.cpp's own
// `b`/`bl` handling.
constexpr uint16_t kPowerPCRelocBranch = 3;

// A from-scratch, in-process two-pass assembler for exactly the
// narrow PowerPC dialect PowerPCCodegen.cpp emits - not a general-
// purpose PowerPC assembler. Parses via the same shared grammar
// ../../../grammar/PccAsm.g4 every other backend's own in-process
// assembler uses (see M68kAssembler.h's own header comment). Every
// operand is a plain expression (a number, a register name, or a
// label) classified by PowerPCCodegen.cpp's own convention, not real
// `0(r4)`/`%hi()`/`%lo()` PowerPC syntax - the grammar has no way to
// express a displacement-from-register memory operand or a function-
// style `%hi`/`%lo` modifier, so every addressing mode this dialect
// can't express as a bare register-or-label operand instead gets its
// own dedicated mnemonic (`ldind`/`stind`, always a fixed zero
// displacement - see PowerPCCodegen.h's own header comment on why
// every load/store here is always through a freshly computed
// address), and `lis`/`ori` resolve a *label* operand (as opposed to
// a plain number) to that label's own upper/lower 16 bits
// automatically instead of needing `%hi`/`%lo` spelled out at all
// (same convention as MipsAssembler.h's own `lui`/`ori`).
//
// Every real instruction encoded here matches genuine 32-bit classic
// PowerPC machine code bit-for-bit (opcodes/XO fields/operand-field
// placement all taken from the real architecture, including the
// well-known asymmetry where a logical instruction's own mnemonic
// operand order - `and rDest,rSrc,rB` - doesn't match its own
// encoded field order, unlike an arithmetic instruction's - see
// encodeXArith()/encodeXLogical()'s own comments) - this dialect only
// narrows which *mnemonics and operand shapes* exist, never what a
// given real encoding means.
//
// Every instruction here is a fixed 4 bytes. Unlike MIPS, PowerPC has
// no branch-delay slot at all, so there's no MIPS-style trailing
// `nop` convention to account for here either.
//
// Throws std::runtime_error on any assembly error.
std::string assemblePowerPC(const std::string &asmText);

// The real separate-compilation entry point (`--format=coff`/`elf`/
// `macho`/`aout` - see PowerPCCodegen.h's own `objectMode`): assembles
// `asmText` (generated with `org 0`, not `kPowerPCBaseAddress` - see
// PowerPCCodegen.cpp's own `generate()`) exactly like
// assemblePowerPC() does, except an undefined symbol is deferred as a
// relocation instead of a hard error, and *every* `lis`/`ori` address
// reference - even to a label defined in this same file - also gets
// an additional relocation against the reserved pseudo-symbol ".."
// (see ../../linker/GenericLinker.h's own header comment on why: this
// text's own labels all resolve to "offset from this object's own
// org 0" until that base is added). A `b`/`bl` branch needs no such
// treatment for a label defined in this same file (see
// `kPowerPCRelocBranch`'s own comment above) - only a genuinely
// external `bl` target gets a relocation at all. Returns the format-
// neutral ObjectFile a codegen backend builds once per compile (see
// ../../fileformat/ObjectFile.h) - this function never touches a
// real COFF/ELF/Mach-O/a.out byte layout itself.
ObjectFile assemblePowerPCObject(const std::string &asmText);

}  // namespace pcc
