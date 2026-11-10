#pragma once

#include <memory>

#include "linker/Linker.h"

namespace rcc {

// Builds a GenericLinker for STM8 (big-endian): Abs16 relocations patched big-endian, and a
// synthesized entry object (set up the stack, `call fn_main`, halt) that is also the only place
// the backend's shared pseudo-registers (ac/bc/cx/dx/ptr/fp/fsp/lc) are defined; every user object
// only references them via external relocation.
std::unique_ptr<TargetLinker> makeSTM8Linker();

}  // namespace rcc
