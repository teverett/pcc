#pragma once

#include <memory>

#include "linker/Linker.h"

namespace rcc {

// Builds a GenericLinker for the PDP-10, with `kPdp10RelocAbs18` patched via this file's own
// read36()/write36()-based 18-bit-field patcher, plus a synthesized `_start` entry object
// assembled like any other object.
std::unique_ptr<TargetLinker> makePdp10Linker();

}  // namespace rcc
