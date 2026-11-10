#pragma once

#include <memory>

#include "linker/Linker.h"

namespace rcc {

// Builds a GenericLinker for the 8008: in addition to the shared patchAbs16, two dedicated
// byte-at-a-time kinds (kI8008RelocAbs16Lo/Hi) patch pointAt()'s byte-split address load.
std::unique_ptr<TargetLinker> makeI8008Linker();

}  // namespace rcc
