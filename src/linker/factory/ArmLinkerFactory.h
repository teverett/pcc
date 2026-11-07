#pragma once

#include <memory>

#include "linker/Linker.h"

namespace pcc {

// Builds a GenericLinker for armv7a (ARM A32): Rel24 for `b`/`bl` and MovWLo/MovTHi for a split
// 32-bit address load are patched by local functions here rather than the shared RelocKinds.
std::unique_ptr<TargetLinker> makeArmLinker();

}  // namespace pcc
