#pragma once

#include <memory>

#include "linker/Linker.h"

namespace pcc {

// Builds a GenericLinker (see GenericLinker.h) configured for the
// MC68328: `kMC68328CoffMachine` (see
// ../codegen/mc68328/MC68328Assembler.h), `kMC68328RelocAbs32`
// patched via the shared `RelocKinds::patchAbs32BE` (big-endian,
// real 68000-family byte order - the MC68328 dialect's own only
// relocation kind, same reasoning ../codegen/z80/Z80LinkerFactory.h's
// own header comment gives for the Z80's own Abs16-only need), and a
// synthesized entry object that lays out the *real* MC68328 reset
// vector table at address 0 (initial SSP, initial PC - see
// MC68328Codegen.h's own header comment on why real DragonBall
// silicon genuinely needs this, unlike ../m68k's own simulator-PC-
// forced entry) followed by `_start` (reload A7 belt-and-suspenders,
// `jsr fn_main`, read the result from D0, halt) - `jsr fn_main`
// becomes a genuine external relocation, resolved exactly like any
// other cross-object call.
//
// This is the one real linked-program base address every object is
// laid out from (0 - the vector table itself) - every user object
// comes right after this entry object's own bytes, sequentially, the
// same convention every GenericLinker-based target uses.
std::unique_ptr<TargetLinker> makeMC68328Linker();

}  // namespace pcc
