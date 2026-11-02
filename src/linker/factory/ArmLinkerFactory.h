#pragma once

#include <memory>

#include "linker/Linker.h"

namespace pcc {

// Builds a GenericLinker (see GenericLinker.h) configured for the
// armv7a (ARM A32) backend: `kArmBaseAddress`/`kArmCoffMachine` (see
// ../../codegen/arm/ArmAssembler.h), this dialect's own three
// relocation kinds (Rel24 for `b`/`bl`, MovWLo/MovTHi for a split
// 32-bit address load - patched by local functions in this file's own
// .cpp, not the shared RelocKinds, same precedent as
// Arm64LinkerFactory.cpp's own Rel26/MovZLo/MovKHi split), and a
// synthesized entry object (`_start`: set `sp` to the initial stack
// pointer, `bl fn_main`, halt via self-branch) assembled exactly like
// any other object - `bl fn_main` becomes a genuine external
// relocation, resolved exactly like any other cross-object call, same
// pattern M68kLinkerFactory.h's own header comment documents.
//
// This backend uses real CPU registers (r0-r3/sp/lr - see
// ArmCodegen.h's own header comment), so no shared-pseudo-register
// fixup is needed here at all.
std::unique_ptr<TargetLinker> makeArmLinker();

}  // namespace pcc
