#pragma once

#include <cstdint>
#include <string>

#include "fileformat/ObjectFile.h"

namespace rcc {

// Custom COFF machine ID; no official 6809 machine ID exists. Mirrors
// kM6502CoffMachine's "chip number as hex" convention.
constexpr uint16_t kM6809CoffMachine = 0x6809;

// Must match M6809Codegen.cpp's kBaseAddress, so a linked program loads at
// the same address as a single-file `--format=raw` one.
constexpr uint32_t kM6809BaseAddress = 0x3000;

// Only relocation kind needed: every absolute reference this backend emits
// is a full 16-bit address (extended addressing or a 16-bit immediate used
// as an address-of). Patched big-endian - see GenericLinker.h's
// RelocKinds::patchAbs16BE, not patchAbs16.
constexpr uint16_t kM6809RelocAbs16 = 1;

// Two-pass, in-process assembler for the subset of real MC6809 assembly
// this backend's codegen emits (no macros, no conditional assembly, no
// direct/page-zero addressing - every absolute reference is extended
// addressing). Produces real, standard 6809 opcodes, valid input for any
// accurate 6809 emulator or disassembler.
//
// Every multi-byte field (16-bit immediate, extended address, long-branch
// relative displacement) is big-endian, matching real 6809 hardware -
// unlike the Z80/8080 dialects in this project, which are little-endian.
//
// `[expr]` always means "the 16-bit address expr names" (extended
// addressing), exactly like the Z80 dialect's own `[nn]` - e.g. `ldd
// [ac]`/`std [ac]` load/store the 16-bit cell at label `ac`. A bare
// (unbracketed) operand on an instruction that can take one is an
// immediate value, e.g. `ldd 1234`/`ldx g_foo` (the latter loading
// `g_foo`'s own address as data, the usual way to materialize a pointer).
//
// Indexed addressing (`LEAX`/`LEAY`/`LEAS`/`LEAU` and the indexed form of
// every load/store/arithmetic/compare mnemonic below) is always written
// as two operands, `<offset>,<reg>` (reg one of x/y/u/s), e.g. `leax
// 4,u`/`lda 0,x`. Only the 8-bit-signed-offset indexed form is supported
// (postbyte `0x88 | regSelect`, then one signed offset byte) - the one
// form this backend's codegen ever needs, since every frame/pointer
// offset it computes fits a signed byte; `offset` outside -128..127 is a
// hard assembly error, not a silently wider encoding.
//
// Register-list operands (`PSHS`/`PULS`/`PSHU`/`PULU`) are written as
// comma-separated register names, e.g. `pshs a,b,x`. `TFR`/`EXG` take two
// bare register names, source first, e.g. `tfr s,u`.
//
// Every conditional branch this backend emits is a *long* branch
// (`LBEQ`/`LBNE`/`LBLT`/`LBGE`/`LBLE`/`LBGT`/`LBLO`/`LBHS`/`LBHI`/`LBLS`,
// plus unconditional `LBRA`) with a full 16-bit relative displacement -
// never a short 8-bit branch - so an instruction's size is fixed by its
// mnemonic alone, the same invariant Z80Assembler.h documents for its own
// dialect. `JMP`/`JSR` are extended addressing (one bracketed operand) or
// indexed (`<offset>,<reg>`, e.g. `jmp 0,x` - transfers control directly to
// the computed address, the only way to call/jump through a register-held
// address); `RTS` is inherent.
//
// `ANDCC`/`ORCC` take a single bare immediate mask, clearing/setting
// condition-code bits directly (e.g. `andcc 0xEF`/`orcc 0x10` mask/unmask
// IRQ via the I flag - this dialect's only equivalent of a dedicated EI/DI).
//
// Throws std::runtime_error on any assembly error.
std::string assembleM6809(const std::string &asmText);

// Entry point for separate compilation (--format=coff/elf/macho/aout).
// Assembles like assembleM6809(), except an undefined symbol becomes a
// relocation instead of a hard error, and every address reference also
// gets a relocation against the reserved pseudo-symbol ".." (this
// object's eventual linked base), since labels here resolve relative to
// `org 0` until a linker adds that base in.
ObjectFile assembleM6809Object(const std::string &asmText);

}  // namespace rcc
