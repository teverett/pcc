#pragma once

#include <memory>

#include "linker/Linker.h"

namespace pcc {

// Builds a GenericLinker (see ../GenericLinker.h) configured for the
// pic16 backend: `kPIC16BaseAddress`/`kPIC16CoffMachine`/
// `kPIC16DataBaseAddress` (see ../../codegen/pic16/PIC16Assembler.h),
// `kPIC16RelocGotoCall`/`kPIC16RelocData` patched via this file's own
// local, masked patch functions (see each one's own comment - neither
// is a plain whole-cell additive patch, so GenericLinker.h's own
// shared `RelocKinds` has nothing reusable here), and a synthesized
// entry object (`_start`: point `SP` at `SSTACK`, `call fn_main`,
// halt) assembled exactly like any other object - `call fn_main`
// becomes a genuine external relocation, resolved exactly like any
// other cross-object call, same pattern Z80LinkerFactory.cpp's own
// header comment documents for z80.
//
// Unlike Z80LinkerFactory.h's own entry object (which is also the
// only place the z80 backend's own shared pseudo-registers are
// defined), this entry object's own `ac`/`bc`/.../`SSTACK` equates
// are just one more copy of the exact same fixed, compile-time
// constants every PIC16 object declares identically (see
// PIC16Codegen.cpp's own `declareRam()`) - no special fixup is needed
// here, same reasoning as ../../codegen/i8008/I8008LinkerFactory.h's
// own entry object.
std::unique_ptr<TargetLinker> makePIC16Linker();

}  // namespace pcc
