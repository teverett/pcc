#pragma once

#include <cstdint>
#include <memory>

#include "codegen/m6502/M6502Assembler.h"
#include "linker/Linker.h"

namespace rcc {

// Builds a GenericLinker for the 6502, with three relocation kinds
// (16-bit absolute plus byte-only Lo/Hi halves) and a synthesized
// `_start` entry object assembled like any other object.
//
// A non-default `baseAddress` links the given objects to run starting
// somewhere other than the ordinary `kM6502BaseAddress` ($0600) - needed
// for a `--format=prodos` image meant to be injected as a ProDOS `SYS`
// file, which ProDOS's own loader always enters at $2000 regardless of
// its directory entry's `aux_type` (see src/fileformat/prodos/
// ProDosFormat.h). Safe at any address above the frame-storage bump
// allocator's own fixed `$0200-$05FF` region (M6502Codegen.cpp's
// `kFrameStackBase`), which this doesn't move - that region is baked
// into the codegen itself, independent of where the linker then places
// the linked program.
std::unique_ptr<TargetLinker> makeM6502Linker(uint32_t baseAddress = kM6502BaseAddress);

// Same instruction encoding, same GenericLinker machinery, but `--target=atari2600`'s own
// zero-page/frame-storage layout (see M6502Codegen.h's own `zeroPageBase()`/`frameStackBase()`
// comments) and its own distinct machine ID (`kM6502Atari2600CoffMachine`, so `--link` rejects
// an accidental mix with a plain 6502 object instead of silently corrupting data) - see
// M6502LinkerFactory.cpp's own `kAtari2600EntryAsm` comment for the entry sequence this links
// in. A non-default `baseAddress` must be `$F000` (4K cartridge) or `$F800` (2K cartridge) to
// produce a valid `--format=a26` image (see src/fileformat/atari2600/Atari2600RomFormat.h).
std::unique_ptr<TargetLinker> makeAtari2600Linker(uint32_t baseAddress = kM6502Atari2600BaseAddress);

// Same instruction encoding, same GenericLinker machinery, and the same plain-6502 zero-page/
// frame-storage layout (the NES's 2A03 has no memory-map hazard like the 2600's TIA - see
// M6502Codegen.h's own header comment), but `--target=2a03`'s own distinct machine ID
// (`kM6502Nes2A03CoffMachine`, so `--link` rejects an accidental mix with a plain 6502 object
// expecting the `$0600` base instead of silently linking one at the wrong address) - see
// M6502LinkerFactory.cpp's own `kNes2A03EntryAsm` comment for the entry sequence this links in.
// A non-default `baseAddress` must leave room for a `$FFFA`-`$FFFF` vector table within the
// cartridge's own PRG-ROM window to produce a valid `--format=nes` image (see src/fileformat/
// nes/NesRomFormat.h).
std::unique_ptr<TargetLinker> makeNes2A03Linker(uint32_t baseAddress = kM6502Nes2A03BaseAddress);

}  // namespace rcc
