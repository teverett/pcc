#pragma once

#include <memory>

#include "linker/Linker.h"

namespace rcc {

// Builds a GenericLinker for the TMS9900: one relocation kind (16-bit absolute - see
// TMS9900Assembler.h's own header comment for why no second kind is needed), and a synthesized
// `_start` entry object (set WP, seed the software stack pointer r15, `bl` into `fn_main`, then
// halt) assembled like any other object.
std::unique_ptr<TargetLinker> makeTMS9900Linker();

}  // namespace rcc
