#pragma once

#include <memory>

#include "linker/Linker.h"

namespace pcc {

// Builds a GenericLinker for MIPS (r4000/r10000), with local patchers
// for the split LUI/ORI address load and the 26-bit J/JAL target
// field, plus a synthesized `_start` entry object assembled like any
// other object.
std::unique_ptr<TargetLinker> makeMipsLinker();

}  // namespace pcc
