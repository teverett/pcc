#pragma once

#include <memory>

#include "linker/Linker.h"

namespace pcc {

// Builds a GenericLinker for PIC18, with a three-byte carry-correct
// split patch for the 20-bit GOTO/CALL word address, a bounds-checked
// single-byte patch for data-space addresses, and a synthesized entry
// object doing real reset-vector setup.
//
// This entry object cannot run any user object's global initializers,
// since it is built before any user object exists; each object's own
// PIC18Codegen::generate() runs its globals' init code in `fn_main`'s
// prologue instead, so every global a file references must be defined
// in that same file.
std::unique_ptr<TargetLinker> makePIC18Linker();

}  // namespace pcc
