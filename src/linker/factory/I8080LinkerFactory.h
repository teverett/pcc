#pragma once

#include <memory>

#include "linker/Linker.h"

namespace rcc {

// Builds a GenericLinker for the 8080. The synthesized entry object is the only place the shared
// pseudo-registers ac/bc/cx/dx/ptr are actually defined; every user object only references them,
// resolved as an ordinary external relocation.
std::unique_ptr<TargetLinker> makeI8080Linker();

}  // namespace rcc
