#pragma once

#include <memory>

#include "linker/Linker.h"

namespace pcc {

// Builds a GenericLinker (see GenericLinker.h) configured for the
// armv8a (AArch64) backend: `kArm64BaseAddress`/`kArm64CoffMachine`
// (see ../../codegen/arm64/Arm64Assembler.h), this dialect's own three
// relocation kinds (Rel26 for `b`/`bl`, MovZLo/MovKHi for a split
// 32-bit address load - patched by local functions in this file's own
// .cpp, not the shared RelocKinds, same precedent as
// M6502LinkerFactory.cpp's own split-byte kinds), and a synthesized
// entry object (`_start`: set `sp` to the initial stack pointer, `bl
// fn_main`, halt via self-branch) assembled exactly like any other
// object - `bl fn_main` becomes a genuine external relocation,
// resolved exactly like any other cross-object call, same pattern
// M68kLinkerFactory.h's own header comment documents.
//
// This backend uses real CPU registers (w0-w2/x0-x2/sp/x30 - see
// Arm64Codegen.h's own header comment), so no shared-pseudo-register
// fixup is needed here at all.
std::unique_ptr<TargetLinker> makeArm64Linker();

}  // namespace pcc
