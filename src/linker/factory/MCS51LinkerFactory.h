#pragma once

#include <memory>

#include "linker/Linker.h"

namespace pcc {

// Builds a GenericLinker (see ../GenericLinker.h) configured for the
// MCS-51 family (8051/8052/8031/8032 - see
// ../../codegen/mcs51/MCS51Codegen.h): `kMCS51BaseAddress`/
// `kMCS51CoffMachine` (see ../../codegen/mcs51/MCS51Assembler.h),
// `kMCS51RelocAbs16` patched via the shared
// `RelocKinds::patchAbs16BE` (this dialect's own only relocation kind
// - see that same header's comment on why there's no split hi/lo
// pair the way i8008/m6502 need), and a synthesized entry object
// (`_start`: redeclare the fixed internal-RAM pseudo-register
// equates, `lcall fn_main`, halt) assembled exactly like any other
// object - `lcall fn_main` becomes a genuine external relocation,
// resolved exactly like any other cross-object call, same pattern
// Z80LinkerFactory.cpp's own header comment documents for z80.
//
// This entry object does *not* run any user object's own global
// variable initializers - it's built once, before it has ever seen
// any user object at all, so it can't know any of them exist. Each
// user object's own MCS51Codegen::generate() already arranges for the
// one defining `main` to run its own globals' init code as
// `fn_main`'s own prologue instead - see that file's own `objectMode`
// comment for why, and for the resulting restriction (every global a
// file references has to be defined in that same file).
std::unique_ptr<TargetLinker> makeMCS51Linker();

}  // namespace pcc
