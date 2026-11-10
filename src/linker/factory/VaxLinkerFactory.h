#pragma once

#include <memory>

#include "linker/Linker.h"

namespace rcc {

// Builds a GenericLinker for the vax backend: one relocation kind (a 32-bit little-endian absolute
// address field), and a synthesized `_start` entry object (set `sp`, `calls fn_main`, `halt`)
// assembled and linked like any other object.
std::unique_ptr<TargetLinker> makeVaxLinker();

}  // namespace rcc
