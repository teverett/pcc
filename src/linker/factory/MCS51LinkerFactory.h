#pragma once

#include <memory>

#include "linker/Linker.h"

namespace rcc {

// Builds a GenericLinker for the MCS-51 family (8051/8052/8031/8032),
// with a synthesized `_start` entry object assembled like any other
// object.
//
// This entry object cannot run any user object's global initializers,
// since it is built before any user object exists; each object's own
// MCS51Codegen::generate() runs its globals' init code in `fn_main`'s
// prologue instead, so every global a file references must be defined
// in that same file.
std::unique_ptr<TargetLinker> makeMCS51Linker();

}  // namespace rcc
