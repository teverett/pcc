#pragma once

#include <cstdint>
#include <memory>

#include "codegen/m65c02/M65C02Assembler.h"
#include "linker/Linker.h"

namespace rcc {

// Builds a GenericLinker for the 65C02, with 16-bit absolute plus
// byte-only Lo8/Hi8 relocation kinds and a synthesized `_start` entry
// object assembled like any other object.
//
// A non-default `baseAddress` links the given objects to run starting
// somewhere other than the ordinary `kM65C02BaseAddress` ($0600) - see
// M6502LinkerFactory.h's own comment on why (a ProDOS `SYS` file's fixed
// $2000 entry point). Safe at any address below the frame-storage area's
// own fixed top-of-memory region (M65C02Codegen.cpp's own `sp` seed,
// $F000), which this doesn't move.
std::unique_ptr<TargetLinker> makeM65C02Linker(uint32_t baseAddress = kM65C02BaseAddress);

}  // namespace rcc
