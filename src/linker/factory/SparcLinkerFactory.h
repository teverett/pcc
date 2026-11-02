#pragma once

#include <memory>

#include "linker/Linker.h"

namespace pcc {

// Builds a GenericLinker (see ../GenericLinker.h) configured for the
// sparc backend: `kSparcBaseAddress`/`kSparcCoffMachine` (see
// ../../codegen/sparc/SparcAssembler.h), this dialect's own three
// relocation kinds (`kSparcRelocHi`/`Lo` for the `sethi`/`or`
// address-load pair, `kSparcRelocCall` for `call` - patched by local
// functions in this file's own .cpp, not the shared `RelocKinds`,
// same precedent as PowerPCLinkerFactory.cpp's own Hi/Lo/Branch
// kinds), and a synthesized entry object (`_start`: set `sp` to the
// initial stack pointer, `call fn_main`, halt via self-branch)
// assembled exactly like any other object - `call fn_main` becomes a
// genuine external relocation, resolved exactly like any other
// cross-object call, same pattern M68kLinkerFactory.h's own header
// comment documents.
//
// This backend uses real CPU registers (g0/o0-o2/o6/o7 - see
// SparcCodegen.h's own header comment), so no shared-pseudo-register
// fixup is needed here at all.
std::unique_ptr<TargetLinker> makeSparcLinker();

}  // namespace pcc
