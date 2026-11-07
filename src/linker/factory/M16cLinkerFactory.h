#pragma once

#include <memory>

#include "linker/Linker.h"

namespace pcc {

// Builds a GenericLinker for the M16C, with a synthesized `_start` entry
// object assembled like any other object so `jsr.w fn_main` resolves as
// an ordinary external relocation.
std::unique_ptr<TargetLinker> makeM16cLinker();

}  // namespace pcc
