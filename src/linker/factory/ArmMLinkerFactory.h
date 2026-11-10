#pragma once

#include <memory>

#include "linker/Linker.h"

namespace rcc {

// Builds a GenericLinker for cortex-m3/m4/m7 (ARMv7-M Thumb/Thumb-2): MovWLo/MovTHi for `MOVW`/
// `MOVT`'s non-contiguous imm4:i:imm3:imm8 split, and Branch for `BL`/wide-`B`'s scrambled
// S:I1:I2:imm10:imm11 field, are patched by local functions here rather than the shared
// RelocKinds.
std::unique_ptr<TargetLinker> makeArmMLinker();

}  // namespace rcc
