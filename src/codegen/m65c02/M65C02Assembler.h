#pragma once

#include <cstdint>
#include <string>

#include "objectformat/ObjectFile.h"

namespace pcc {

// This target's own custom, non-registered COFF/object-format
// machine ID (no real one exists for the 65C02 - see
// ../../coff/CoffWriter.h's own header comment) - exported so
// ../../linker/M65C02Linker.cpp can check an input object's embedded
// machine type against `--target=65c02` without duplicating this
// literal. Distinct from ../m6502/M6502Assembler.h's own
// `kM6502CoffMachine` (0x6502) even though the two backends share
// almost everything else - a 6502 object and a 65C02 object are
// never meant to link together (the CMOS-only `stzp` opcode
// M65C02Codegen.cpp emits would be meaningless on real NMOS
// hardware), so a mismatched link attempt should fail loudly rather
// than silently succeed.
constexpr uint16_t kM65C02CoffMachine = 0xC502;  // "C502" = CMOS + 6502

// Where the whole program loads and starts running once real
// separate compilation (`--format=coff`/`elf`/`macho`/`aout`) and
// `--link` are both involved - the same address
// M65C02Codegen.cpp's own whole-program `--format=raw` entry
// sequence already uses (`kBaseAddress`), kept identical so a linked
// program behaves exactly like a single-file one. Exported so
// M65C02Linker.cpp's own sequential object layout starts from the
// same place.
constexpr uint32_t kM65C02BaseAddress = 0x0600;

// This dialect's one relocation kind: a little-endian 16-bit
// *additive* patch (see ../../linker/GenericLinker.h's own
// `RelocKinds::patchAbs16`) - every JSR/JMP this backend ever emits
// is a full absolute address (see M65C02Codegen.h's own header
// comment: short branches are always `safeBranch()`-guarded to a
// local, same-file label, never a cross-file reference), so this is
// the only relocation kind the 65C02 dialect needs at all. Note that
// `ptr`/`ac`/`bc`/`cx`/`dx` - the fixed zero-page pseudo-registers
// M65C02Codegen.cpp's own `generate()` binds via `name = value`
// equates - are deliberately *not* relocatable: an Equate's own
// value is a fixed compile-time constant (0/2/4/6/8), identical in
// every object regardless of where it's linked, so (unlike a real
// position-dependent label) a reference to one never needs the
// reserved ".." base-shift relocation - see
// M65C02Assembler.cpp's own `equates_` comment.
constexpr uint16_t kM65C02RelocAbs16 = 1;

// Two more relocation kinds this dialect needs that z80's own
// template didn't: `ldai`/`ldai_hi` (see M65C02Codegen.cpp's own
// `genAddrOf()`/`genIndexAddr()`) take an address-of's own low byte
// and high byte as two *separate* one-byte immediate operands (a
// real `#<label`/`#>label`, just spelled as plain mnemonics - see
// M65C02Assembler.cpp's own `emitImmLow`/`emitImmHigh` comment) -
// each needs its own one-byte additive patch on its own half of the
// resolved 16-bit address, not the two-byte `patchAbs16` above.
// Implemented as local patch functions in M65C02Linker.cpp itself
// (not added to GenericLinker.h's own shared RelocKinds - this
// particular 8-bit-immediate-address-split shape is specific to the
// 6502/65C02 family's own real instruction set, not a shape most
// other GenericLinker-based targets would ever need).
constexpr uint16_t kM65C02RelocAbsLo8 = 2;
constexpr uint16_t kM65C02RelocAbsHi8 = 3;

// A from-scratch, in-process two-pass assembler for exactly the
// narrow WDC 65C02 dialect M65C02Codegen.cpp emits - not a general-
// purpose 65C02 assembler. This is ../m6502/M6502Assembler.cpp's own
// dialect plus exactly one CMOS-only addition: `stz`/`stzp` (STZ,
// absolute/zero-page - see M65C02Codegen.cpp's own comment on where
// that's used and why); everything else here is identical to that
// file; see its own header comment for the shared rationale. Parses
// via the same shared grammar ../../../grammar/PccAsm.g4 the other
// backends' own in-process assemblers use (see Pdp7Assembler.h/
// I8086Assembler.h's own header comments) - every operand here is a
// plain expression (a number, a symbol, or a signed combination of
// them); there's no register operand syntax at all (every mnemonic
// already implies which real 65C02 register(s) it touches - see
// M65C02Codegen.cpp's own comment for why), and no bracketed memory-
// operand syntax either (the one addressing mode that needs it,
// zero-page indirect indexed, has its own dedicated mnemonics -
// `ldaind`/`staind` - instead).
//
// `--format=raw` (whole program) has no relocations at all - every
// symbol must resolve within the one file being assembled, a hard
// error otherwise. `--format=coff`/`elf`/`macho`/`aout`/`objraw`
// (real separate compilation - see M65C02Codegen.h's own
// `objectMode`) instead defer an unresolved symbol as a relocation -
// see `assembleM65C02Object()` below.
//
// Every instruction's *size* is fixed by its mnemonic alone, never
// by any operand's resolved value - same convention, and the same
// reason (a stable two-pass size computation with no phase-sizing
// ambiguity), as ../pdp7/Pdp7Assembler.h and
// ../x86_8086/I8086Assembler.h's own versions of this comment.
//
// Throws std::runtime_error on any assembly error.
std::string assembleM65C02(const std::string &asmText);

// The real separate-compilation entry point (`--format=coff`/`elf`/
// `macho`/`aout`/`objraw` - see M65C02Codegen.h's own `objectMode`):
// assembles `asmText` (generated with `org 0`, not `kBaseAddress` -
// see M65C02Codegen.cpp's own `generate()`) exactly like
// assembleM65C02() does, except an undefined symbol is deferred as a
// relocation instead of a hard error, and every *ordinary* address
// reference (a real label, never one of the fixed zero-page equates -
// see kM65C02RelocAbs16's own comment) also gets an additional
// relocation against the reserved pseudo-symbol ".." (this object's
// own eventual linked base - see ../../linker/GenericLinker.h's own
// header comment), the same convention ../z80/Z80Assembler.h's own
// `assembleZ80Object` documents. Returns the format-neutral
// ObjectFile a codegen backend builds once per compile (see
// ../../objectformat/ObjectFile.h) - this function never touches a
// real COFF/ELF/Mach-O/a.out byte layout itself.
ObjectFile assembleM65C02Object(const std::string &asmText);

}  // namespace pcc
