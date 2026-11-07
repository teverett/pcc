#pragma once

#include <memory>

#include "linker/Linker.h"

namespace pcc {

// Builds a GenericLinker for Z80: Abs16 relocations, and a synthesized entry object (set up the
// stack, `call fn_main`, halt) that is also the only place the backend's shared pseudo-registers
// (ac/_bc/cx/dx/ptr) are defined; every user object only references them via external relocation.
std::unique_ptr<TargetLinker> makeZ80Linker();

}  // namespace pcc
