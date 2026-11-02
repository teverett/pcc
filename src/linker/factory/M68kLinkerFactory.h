#pragma once

#include <memory>

#include "linker/Linker.h"

namespace pcc {

// Builds a GenericLinker (see GenericLinker.h) configured for the
// m68k (68000) backend: `kM68kBaseAddress`/`kM68kCoffMachine` (see
// ../codegen/m68k/M68kAssembler.h), `kM68kRelocAbs32` patched via the
// shared `RelocKinds::patchAbs32BE` (this target is real big-endian
// 68000 hardware - see that same header's comment on why there's only
// this one relocation kind), and a synthesized entry object (`_start`:
// set A7 to `kInitialSP`, `jsr fn_main`, halt via self-branch)
// assembled exactly like any other object - `jsr fn_main` becomes a
// genuine external relocation, resolved exactly like any other
// cross-object call, same pattern Z80LinkerFactory.cpp's own header
// comment documents for z80.
//
// Unlike Z80LinkerFactory.h's own entry object (which is also the
// only place the z80 backend's own shared pseudo-registers are
// defined), this backend uses real CPU registers (D0/D1/A0/A7 - see
// M68kCodegen.h's own header comment), so no such fixup is needed
// here at all.
std::unique_ptr<TargetLinker> makeM68kLinker();

}  // namespace pcc
