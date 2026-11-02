#pragma once

#include <cstdint>
#include <string>

#include "objectformat/ObjectFile.h"

namespace pcc {

// This target's own custom, non-registered COFF/object-format
// machine ID (no real one exists for the 6502 - see
// ../../coff/CoffWriter.h's own header comment) - exported so
// ../../linker/M6502Linker.cpp can check an input object's embedded
// machine type against `--target=6502` without duplicating this
// literal.
constexpr uint16_t kM6502CoffMachine = 0x6502;

// Where the whole program loads and starts running once real
// separate compilation (`--format=coff`/`elf`/`macho`/`aout`, see
// `allowExternal` below) and `--link` are both involved - the same
// address M6502Codegen.cpp's own whole-program `--format=raw` entry
// sequence already uses (`kBaseAddress`), kept identical so a linked
// program behaves exactly like a single-file one. Exported so
// M6502Linker.cpp's own sequential object layout starts from the
// same place.
constexpr uint32_t kM6502BaseAddress = 0x0600;

// This dialect's own relocation kinds. Unlike z80 (which only ever
// needs one - see Z80Assembler.h's own comment), this one needs
// three: every `jsr`/`jmp`/absolute `lda`/`sta`/etc. operand
// (M6502Assembler.cpp's own `emitAbsOperand`) is a full 16-bit field,
// same role as Abs16 everywhere else - but `ldai`/`ldai_hi`/`adci`/
// `adci_hi` applied to a label (M6502Codegen.h's own header comment
// on why a real 6502 `LDA #imm8` can only ever load *one* byte at a
// time, so getting an address into a register pair means splitting
// it into its own low and high byte at assemble time - see
// genAddrOf()/genIndexAddr()'s own comments) each patch only *one*
// byte of the resolved 16-bit value, so a plain additive Abs16 patch
// can't be reused for either half.
constexpr uint16_t kM6502RelocAbs16 = 1;
constexpr uint16_t kM6502RelocAbs16Lo = 2;  // low byte only (value & 0xFF)
constexpr uint16_t kM6502RelocAbs16Hi = 3;  // high byte only (value >> 8)

// A from-scratch, in-process two-pass assembler for exactly the
// narrow 6502 dialect M6502Codegen.cpp emits - not a general-purpose
// 6502 assembler. Parses via the same shared grammar
// ../../../grammar/PccAsm.g4 the other two backends' own in-process
// assemblers use (see Pdp7Assembler.h/I8086Assembler.h's own header
// comments) - every operand here is a plain expression (a number, a
// symbol, or a signed combination of them); there's no register
// operand syntax at all (every mnemonic already implies which real
// 6502 register(s) it touches - see M6502Codegen.cpp's own comment
// for why), and no bracketed memory-operand syntax either (the one
// addressing mode that needs it, zero-page indirect indexed, has its
// own dedicated mnemonics - `ldaind`/`staind` - instead).
//
// Every instruction's *size* is fixed by its mnemonic alone, never
// by any operand's resolved value - same convention, and the same
// reason (a stable two-pass size computation with no phase-sizing
// ambiguity), as ../pdp7/Pdp7Assembler.h and
// ../x86_8086/I8086Assembler.h's own versions of this comment.
//
// Throws std::runtime_error on any assembly error.
std::string assembleM6502(const std::string &asmText);

// The real separate-compilation entry point (`--format=coff`/`elf`/
// `macho`/`aout` - see M6502Codegen.h's own `objectMode`) - see
// ../z80/Z80Assembler.h's own `assembleZ80Object` comment for the
// full rationale (identical here): `asmText` is assembled with
// `org 0`, an undefined symbol is deferred as a relocation instead of
// a hard error, and *every* address reference also gets a relocation
// against the reserved pseudo-symbol ".." for this object's own
// eventual linked base. Returns the format-neutral ObjectFile a
// codegen backend builds once per compile (see
// ../../objectformat/ObjectFile.h) - this function never touches a
// real COFF/ELF/Mach-O/a.out byte layout itself.
ObjectFile assembleM6502Object(const std::string &asmText);

}  // namespace pcc
