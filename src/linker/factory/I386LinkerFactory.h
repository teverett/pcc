#pragma once

#include <memory>

#include "linker/Linker.h"

namespace rcc {

// Builds a GenericLinker for i386. The synthesized entry object owns the Multiboot header (which
// must be first in the linked image) and the PIC-masking sequence (which must run before any user
// code that might call `_ion()`), followed by `call fn_main` and a halt loop.
std::unique_ptr<TargetLinker> makeI386Linker();

}  // namespace rcc
