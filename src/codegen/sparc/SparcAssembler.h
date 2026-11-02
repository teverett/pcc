#pragma once

#include <cstdint>
#include <string>

#include "fileformat/ObjectFile.h"

namespace pcc {

// This dialect's own custom, non-registered COFF/object-format
// machine ID (no real one exists for this project's own SPARC
// dialect - see ../../fileformat/FileFormat.cpp's own
// `kKnownCoffMachines`) - exported so
// ../../linker/factory/SparcLinkerFactory.cpp can check an input
// object's embedded machine type against `--target=sparc` without
// duplicating this literal. ASCII "SP", the same ad-hoc
// "recognizably derived from this target's own name" convention as
// z80's own 0x5A38 ("Z8") and powerpc's own 0x5043 ("PC").
constexpr uint16_t kSparcCoffMachine = 0x5350;

// Where the whole program loads and starts running once real
// separate compilation (`--format=coff`/`elf`/`macho`/`aout`, see
// `allowExternal` below) and `--link` are both involved - the same
// address SparcCodegen.cpp's own (private) whole-program
// `--format=raw` entry sequence already uses (`kBaseAddress`), kept
// identical (duplicated here, same convention as PowerPCAssembler.h's
// own `kPowerPCBaseAddress`) so a linked program behaves exactly like
// a single-file one, and so SparcLinkerFactory.cpp's own sequential
// object layout starts from the same place.
constexpr uint32_t kSparcBaseAddress = 0x00004000u;

// This dialect's three relocation kinds - all local to this target,
// not shared via ../../linker/GenericLinker.h's own `RelocKinds` (see
// SparcLinkerFactory.cpp's own patch functions for why: `sethi`/`or`'s
// split address load and `call`'s own 30-bit word-aligned PC-relative
// field aren't a plain contiguous-field additive patch the way every
// other migrated backend's own relocation kind is) - same precedent
// as PowerPCLinkerFactory.cpp's own Hi/Lo/Branch kinds, which this is
// ported from.
//
// `kSparcRelocHi`/`kSparcRelocLo`: the upper 22 bits / lower 10 bits
// of a full 32-bit address, as split across a real `sethi
// %hi(addr),rd` / `or rd,%lo(addr),rd` pair (see
// SparcAssembler.cpp's own `evalHiLo()`) - always emitted as `sethi`
// immediately followed by `or` (4 bytes apart - see
// SparcCodegen.cpp's own `loadAddress()`), so
// SparcLinkerFactory.cpp's own Hi patch function can find its paired
// Lo site at a fixed `+4` offset to do carry-correct 32-bit
// patching. Real SPARC `sethi` always zeroes the low 10 bits of `rd`
// itself and `or` combines the two halves with a bitwise OR, not an
// add, so there's no classic PowerPC-style `@ha`/`@l` sign-extension
// correction to make at the *instruction* level - but the *linker's*
// own correction (this object's real base, for `..`, or an external
// symbol's own full address) still has to be added to each field
// separately, and that addition can still carry out of the 10-bit Lo
// field into the 22-bit Hi field - see PowerPCAssembler.h's own
// identical comment (and the i8008 migration's own
// `patchAbs16Hi`/`patchAbs16Lo` pair) for the identical carry issue
// and the identical fix.
constexpr uint16_t kSparcRelocHi = 1;
constexpr uint16_t kSparcRelocLo = 2;
// `kSparcRelocCall`: `call`'s own 30-bit word-aligned, PC-relative
// displacement field (see SparcAssembler.cpp's own `emitCall()`) - a
// real SPARC CALL instruction's target is always relative to its
// *own* address (`target = PC + sign_extend(disp30) * 4`, no "+8"
// pipeline quirk the way classic ARM A32 has - see
// PowerPCAssembler.h's own identical `kPowerPCRelocBranch` comment),
// so a call to a label defined in *this same object* is already a
// valid displacement regardless of where this object ends up linked
// and never needs a relocation at all; only a genuinely cross-object
// `call` - calling a function defined in some *other* input file -
// needs deferring.
constexpr uint16_t kSparcRelocCall = 3;

// A from-scratch, in-process two-pass assembler for exactly the
// narrow 32-bit SPARC V8 dialect SparcCodegen.cpp emits - not a
// general-purpose SPARC assembler. Parses via the same shared grammar
// ../../../grammar/PccAsm.g4 every other backend's own in-process
// assembler uses (see PowerPCAssembler.h's own header comment). Every
// operand is a plain expression (a number, a register name, or a
// label) classified by SparcCodegen.cpp's own convention, not real
// `[%rs1+offset]`/`%hi()`/`%lo()` SPARC syntax - the grammar has no
// way to express a bracketed base+displacement memory operand (this
// dialect's own grammar-level `LBRACKET` support, used by some other
// backends, is deliberately left unused here) or a function-style
// `%hi`/`%lo` modifier, so every addressing mode this dialect can't
// express as a bare register-or-label operand instead gets its own
// dedicated mnemonic (`ldind`/`stind`, always a fixed zero
// displacement - see SparcCodegen.h's own header comment on why every
// load/store here is always through a freshly computed address), and
// `sethi`/`or` resolve a *label* operand (as opposed to a plain
// number) to that label's own upper 22 bits / lower 10 bits
// automatically instead of needing `%hi`/`%lo` spelled out at all
// (same convention as PowerPCAssembler.h's own `lis`/`ori`).
//
// Every real instruction encoded here matches genuine 32-bit SPARC V8
// machine code bit-for-bit (the op/op2/op3 fields and operand
// placement are all taken from the real architecture) - this dialect
// only narrows which *mnemonics and operand shapes* exist, never what
// a given real encoding means. Every instruction here is a fixed 4
// bytes, except the five control-transfer mnemonics (`call`, `ba`,
// and the six `b<cc>` conditional branches, plus `retl`), each of
// which is always exactly 8 bytes: the real branch/call word itself,
// immediately followed by a real `nop` this assembler always
// synthesizes into the delay slot - see SparcCodegen.h's own header
// comment on why this dialect never needs a useful instruction in a
// delay slot at all, simplest-and-always-correct over squeezing out
// the slot's own real-hardware performance.
//
// Throws std::runtime_error on any assembly error.
std::string assembleSparc(const std::string &asmText);

// The real separate-compilation entry point (`--format=coff`/`elf`/
// `macho`/`aout` - see SparcCodegen.h's own `objectMode`): assembles
// `asmText` (generated with `org 0`, not `kSparcBaseAddress` - see
// SparcCodegen.cpp's own `generate()`) exactly like assembleSparc()
// does, except an undefined symbol is deferred as a relocation
// instead of a hard error, and *every* `sethi`/`or` address reference -
// even to a label defined in this same file - also gets an additional
// relocation against the reserved pseudo-symbol ".." (see
// ../../linker/GenericLinker.h's own header comment on why: this
// text's own labels all resolve to "offset from this object's own
// org 0" until that base is added). A `call` needs no such treatment
// for a label defined in this same file (see `kSparcRelocCall`'s own
// comment above) - only a genuinely external `call` target gets a
// relocation at all. Returns the format-neutral ObjectFile a codegen
// backend builds once per compile (see ../../fileformat/ObjectFile.h) -
// this function never touches a real COFF/ELF/Mach-O/a.out byte
// layout itself.
ObjectFile assembleSparcObject(const std::string &asmText);

}  // namespace pcc
