#pragma once

#include <memory>

#include "linker/Linker.h"

namespace pcc {

// Builds a GenericLinker for the powerpc backend: relocation kinds for the `lis`/`ori`
// address-load pair and `b`/`bl` branches, and a synthesized `_start` entry object (`bl fn_main`,
// halt via self-branch) assembled and linked like any other object.
std::unique_ptr<TargetLinker> makePowerPCLinker();

}  // namespace pcc
