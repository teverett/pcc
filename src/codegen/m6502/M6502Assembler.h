#pragma once

#include <cstdint>
#include <string>

#include "fileformat/ObjectFile.h"

namespace rcc {

// No real COFF machine ID exists for the 6502; this is a project-local stand-in.
constexpr uint16_t kM6502CoffMachine = 0x6502;

// `--target=atari2600`'s own machine ID, distinct from kM6502CoffMachine -
// same instruction encoding, but the zero-page/frame-storage layout the
// codegen and the entry stub both assume differs (see M6502Codegen.h's
// own `zeroPageBase()`/`frameStackBase()` comments), so linking an
// ordinary 6502 object against an atari2600 one (or vice versa) would
// silently corrupt data rather than just fail to link - GenericLinker's
// own machine-type check (see GenericLinker.cpp) rejects the mismatch
// outright instead, the same protection kI8086DosCoffMachine gives
// `--target=8086-dos` against plain `8086`.
constexpr uint16_t kM6502Atari2600CoffMachine = 0x2600;

// `--target=2a03`'s own machine ID, same reasoning as
// kM6502Atari2600CoffMachine above even though the 2A03's zero-page/
// frame-storage layout is identical to plain 6502's (see M6502Codegen.h's
// own header comment) - kept distinct anyway so a NROM-base-address
// (`$8000`) object never silently links against a plain-6502 one
// expecting `$0600`.
constexpr uint16_t kM6502Nes2A03CoffMachine = 0x2A03;

// Linked base address; matches the whole-program raw entry's kBaseAddress.
constexpr uint32_t kM6502BaseAddress = 0x0600;
// atari2600's own default link base: the conventional 4K-cartridge
// address $F000 (see M6502LinkerFactory.cpp's own `kAtari2600EntryAsm`
// comment for why $F000 rather than the real $1000-$1FFF cartridge
// range).
constexpr uint32_t kM6502Atari2600BaseAddress = 0xF000;
// 2a03's own default link base: NROM's fixed 32KB PRG-ROM window,
// $8000-$FFFF (see M6502LinkerFactory.cpp's own `kNes2A03EntryAsm`
// comment).
constexpr uint32_t kM6502Nes2A03BaseAddress = 0x8000;

// Three reloc kinds because LDA takes only an 8-bit immediate: loading a
// 16-bit address into a register pair needs separate low/high-byte patches.
constexpr uint16_t kM6502RelocAbs16 = 1;
constexpr uint16_t kM6502RelocAbs16Lo = 2;  // low byte only (value & 0xFF)
constexpr uint16_t kM6502RelocAbs16Hi = 3;  // high byte only (value >> 8)

// In-process two-pass assembler for the narrow 6502 dialect M6502Codegen.cpp
// emits, not a general-purpose 6502 assembler. No register operand syntax
// (each mnemonic implies its own register) and no bracketed memory operands
// (zero-page indirect indexed uses dedicated `ldaind`/`staind` mnemonics).
// Every instruction's size is fixed by its mnemonic alone, independent of
// operand values, so a stable two-pass size computation needs no fixup pass.
//
// Throws std::runtime_error on any assembly error.
std::string assembleM6502(const std::string &asmText);

// Separate-compilation entry point: assembles with `org 0`, defers undefined
// symbols as relocations, and emits a relocation against pseudo-symbol ".."
// for every local address reference (resolved once linked). Returns a
// format-neutral ObjectFile; this function never writes real object bytes.
// `machineType` is stamped into the returned object (see
// kM6502Atari2600CoffMachine's own comment for why atari2600 passes a
// different one than the default).
ObjectFile assembleM6502Object(const std::string &asmText, uint16_t machineType = kM6502CoffMachine);

}  // namespace rcc
