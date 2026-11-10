#pragma once

#include <memory>

#include "linker/Linker.h"

namespace rcc {

// Builds a GenericLinker for the MC68328 (DragonBall), big-endian. The
// synthesized entry object lays out the real reset vector table
// (initial SSP, initial PC) at address 0, since real silicon needs it,
// followed by `_start`.
std::unique_ptr<TargetLinker> makeMC68328Linker();

}  // namespace rcc
