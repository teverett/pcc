#pragma once

#include <memory>

#include "linker/Linker.h"

namespace pcc {

// Builds a GenericLinker (see GenericLinker.h) configured for the
// pdp11 backend: `kPdp11BaseAddress`/`kPdp11CoffMachine` (see
// ../codegen/pdp11/Pdp11Assembler.h), `kPdp11RelocAbs16` patched via
// the shared `RelocKinds::patchAbs16` (`lea`'s own true absolute
// reference) and `kPdp11RelocRel16` patched via this file's own
// locally-scoped relative-patch function (every other bare-label
// operand, including `jsr` - see Pdp11Assembler.h's own header
// comment on why its formula, `value - siteFinalAddr` with no +2,
// differs from GenericLinker.h's own shared `RelocKinds::patchRel16`),
// and a synthesized entry object (`_start`: set SP to `kInitialSp`,
// `jsr fn_main`, halt via self-branch) assembled exactly like any
// other object - `jsr fn_main` becomes a genuine external relocation,
// resolved exactly like any other cross-object call, same pattern
// Z80LinkerFactory.cpp's own header comment documents for z80.
//
// Unlike Z80LinkerFactory.h's own entry object (which is also the
// only place the z80 backend's own shared pseudo-registers are
// defined), this backend uses real CPU registers (R0-R7/SP - see
// Pdp11Codegen.h's own header comment), so no such fixup is needed
// here at all.
std::unique_ptr<TargetLinker> makePdp11Linker();

}  // namespace pcc
