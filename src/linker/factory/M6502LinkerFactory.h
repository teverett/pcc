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

}  // namespace rcc
