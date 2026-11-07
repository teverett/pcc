#pragma once

#include <memory>

#include "linker/Linker.h"

namespace pcc {

// Builds a GenericLinker for the sparc backend: relocation kinds for the `sethi`/`or` address-load
// pair and `call`, and a synthesized `_start` entry object (`call fn_main`, halt via self-branch)
// assembled and linked like any other object.
std::unique_ptr<TargetLinker> makeSparcLinker();

}  // namespace pcc
