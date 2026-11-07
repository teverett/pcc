#pragma once

#include <memory>

#include "linker/Linker.h"

namespace pcc {

// Builds a GenericLinker for the PDP-11, with `kPdp11RelocAbs16`
// patched via the shared RelocKinds::patchAbs16 and `kPdp11RelocRel16`
// via this file's own PC-relative patcher, plus a synthesized `_start`
// entry object assembled like any other object.
std::unique_ptr<TargetLinker> makePdp11Linker();

}  // namespace pcc
