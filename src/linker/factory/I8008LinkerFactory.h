#pragma once

#include <memory>

#include "linker/Linker.h"

namespace pcc {

// Builds a GenericLinker (see ../GenericLinker.h) configured for the
// 8008: `kI8008BaseAddress`/`kI8008CoffMachine` (see
// ../../codegen/i8008/I8008Assembler.h), `kI8008RelocAbs16` patched
// via the shared `RelocKinds::patchAbs16`, plus two dedicated byte-
// at-a-time kinds (`kI8008RelocAbs16Lo`/`Hi`, local to this file - no
// other GenericLinker-based target but ../../codegen/m6502 needs this
// exact shape, and that one keeps its own copy too rather than
// sharing one - see ../GenericLinker.h's own header comment on when
// a kind belongs there vs. locally) for `pointAt()`'s own byte-split
// address load. A synthesized entry object (`_start`: set up the
// software stack pointer, `cal fn_main`, halt) replicates
// I8008Codegen.cpp's own whole-program entry sequence exactly,
// including re-declaring the `ac`/`bc`/.../`sp` equates every object
// already declares identically (harmless - see that file's own
// `generate()` comment on why no special per-object handling is
// needed for them at all).
std::unique_ptr<TargetLinker> makeI8008Linker();

}  // namespace pcc
