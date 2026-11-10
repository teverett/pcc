#pragma once

#include <memory>

#include "linker/Linker.h"

namespace rcc {

// Builds a GenericLinker for Alpha AXP, with local patchers for the sign-extending `ldah`/`lda`
// split address load (see AlphaAssembler.h) plus a synthesized `_start` entry object assembled
// like any other object.
std::unique_ptr<TargetLinker> makeAlphaLinker();

}  // namespace rcc
