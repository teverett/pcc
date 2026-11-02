#pragma once

#include <memory>

#include "linker/Linker.h"

namespace pcc {

// Builds a GenericLinker (see GenericLinker.h) configured for the
// 65C02: `kM65C02BaseAddress`/`kM65C02CoffMachine` (see
// ../codegen/m65c02/M65C02Assembler.h), `kM65C02RelocAbs16` patched
// via the shared `RelocKinds::patchAbs16`, plus two local, dialect-
// specific patch functions for `kM65C02RelocAbsLo8`/
// `kM65C02RelocAbsHi8` (the one-byte low/high halves of a `genAddrOf`
// address - see M65C02Assembler.h's own header comment on why these
// aren't shared GenericLinker-wide RelocKinds), and a synthesized
// entry object (`_start`: `jsr fn_main`, read the result into `a`,
// halt) assembled exactly like any other object.
//
// Unlike ../codegen/z80/Z80Linker.h's own entry object, this one does
// *not* need to define `ptr`/`ac`/`bc`/`cx`/`dx` - every object
// (including this entry one) already declares them identically via
// a fixed compile-time Equate, never a real relocatable address (see
// M65C02Codegen.cpp's own `generate()` comment) - so there's nothing
// for this linker to coordinate for them at all.
std::unique_ptr<TargetLinker> makeM65C02Linker();

}  // namespace pcc
