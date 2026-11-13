#pragma once

#include <memory>

#include "linker/Linker.h"

namespace rcc {

// Builds a GenericLinker for 6809: Abs16 relocations (big-endian), and a synthesized entry object
// (set up the stack, `jsr fn_main`, halt) that is also the only place the backend's shared
// pseudo-registers (ac/_bc/cx/dx/ptr) are defined; every user object only references them via
// external relocation.
std::unique_ptr<TargetLinker> makeM6809Linker();

}  // namespace rcc
