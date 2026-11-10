#pragma once

#include <memory>

#include "linker/Linker.h"

namespace rcc {

// Builds a GenericLinker for the 65C02, with 16-bit absolute plus
// byte-only Lo8/Hi8 relocation kinds and a synthesized `_start` entry
// object assembled like any other object.
std::unique_ptr<TargetLinker> makeM65C02Linker();

}  // namespace rcc
