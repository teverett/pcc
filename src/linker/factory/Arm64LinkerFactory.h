#pragma once

#include <memory>

#include "linker/Linker.h"

namespace pcc {

// Builds a GenericLinker for armv8a (AArch64): Rel26 for `b`/`bl` and MovZLo/MovKHi for a split
// 32-bit address load are patched by local functions here rather than the shared RelocKinds. The
// synthesized entry object's `bl fn_main` resolves as an ordinary external relocation.
std::unique_ptr<TargetLinker> makeArm64Linker();

}  // namespace pcc
