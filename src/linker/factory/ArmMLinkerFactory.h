#pragma once

#include <memory>

#include "linker/Linker.h"

namespace pcc {

// Builds a GenericLinker (see GenericLinker.h) configured for the
// cortex-m3/cortex-m4/cortex-m7 (ARMv7-M Thumb/Thumb-2) backend:
// `kArmMBaseAddress`/`kArmMCoffMachine` (see
// ../../codegen/armm/ArmMAssembler.h), this dialect's own three
// relocation kinds (MovWLo/MovTHi for `MOVW`/`MOVT`'s own
// non-contiguous imm4:i:imm3:imm8 split, Branch for `BL`/wide-`B`'s
// own S:I1:I2:imm10:imm11 scrambled field - patched by local
// functions in this file's own .cpp, not the shared RelocKinds, same
// precedent as ../../codegen/arm64/Arm64LinkerFactory.cpp's own
// Rel26/MovZLo/MovKHi split), and a synthesized entry object
// (`_start`: set `sp` to the initial stack pointer via `MOVW`+`MOVT`,
// `bl fn_main`, halt via self-branch) assembled exactly like any
// other object - `bl fn_main` becomes a genuine external relocation,
// resolved exactly like any other cross-object call, same pattern
// M68kLinkerFactory.h's own header comment documents.
//
// This backend uses real CPU registers (r0-r3/sp/lr - see
// ArmMCodegen.h's own header comment), so no shared-pseudo-register
// fixup is needed here at all.
std::unique_ptr<TargetLinker> makeArmMLinker();

}  // namespace pcc
