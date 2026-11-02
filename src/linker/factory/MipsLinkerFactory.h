#pragma once

#include <memory>

#include "linker/Linker.h"

namespace pcc {

// Builds a GenericLinker (see GenericLinker.h) configured for the
// r10000/r4000 (MIPS) backend: `kMipsBaseAddress`/`kMipsCoffMachine`
// (see ../../codegen/mips/MipsAssembler.h), `kMipsRelocHi`/
// `kMipsRelocLo`/`kMipsRelocJump` patched via this file's own local
// patch functions - see GenericLinker.h's own header comment on why
// MIPS's split LUI/ORI address load and its own 26-bit pseudo-
// absolute J/JAL target field aren't a plain contiguous-field
// additive patch the way every RelocKinds-backed target's own
// relocation kind is - and a synthesized entry object (`_start`: set
// `$sp`, `jal fn_main`, halt via self-branch) assembled exactly like
// any other object - `jal fn_main` becomes a genuine external
// relocation, resolved exactly like any other cross-object call, same
// pattern M68kLinkerFactory.h's own header comment documents for
// 68000.
std::unique_ptr<TargetLinker> makeMipsLinker();

}  // namespace pcc
