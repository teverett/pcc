#pragma once

#include <memory>

#include "linker/Linker.h"

namespace pcc {

// Builds a GenericLinker (see ../GenericLinker.h) configured for the
// powerpc backend: `kPowerPCBaseAddress`/`kPowerPCCoffMachine` (see
// ../../codegen/powerpc/PowerPCAssembler.h), this dialect's own three
// relocation kinds (`kPowerPCRelocHi`/`Lo` for the `lis`/`ori`
// address-load pair, `kPowerPCRelocBranch` for `b`/`bl` - patched by
// local functions in this file's own .cpp, not the shared
// `RelocKinds`, same precedent as Arm64LinkerFactory.cpp's own
// Rel26/MovZLo/MovKHi kinds), and a synthesized entry object
// (`_start`: set `sp` to the initial stack pointer, `bl fn_main`, halt
// via self-branch) assembled exactly like any other object - `bl
// fn_main` becomes a genuine external relocation, resolved exactly
// like any other cross-object call, same pattern M68kLinkerFactory.h's
// own header comment documents.
//
// This backend uses real CPU registers (r0/r1/r3/r4/r5 - see
// PowerPCCodegen.h's own header comment), so no shared-pseudo-register
// fixup is needed here at all.
std::unique_ptr<TargetLinker> makePowerPCLinker();

}  // namespace pcc
