#pragma once

#include <memory>

#include "linker/Linker.h"

namespace rcc {

// Builds a GenericLinker shared by the 8086/8088 backend. Plain byte-addressed little-endian
// real-mode x86, so relocations patch via the shared RelocKinds with no local patch function.
std::unique_ptr<TargetLinker> makeI8086Linker();

}  // namespace rcc
