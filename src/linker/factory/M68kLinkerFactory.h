#pragma once

#include <memory>

#include "linker/Linker.h"

namespace rcc {

// Builds a GenericLinker for the m68k (68000), big-endian, with a
// synthesized `_start` entry object assembled like any other object.
std::unique_ptr<TargetLinker> makeM68kLinker();

}  // namespace rcc
