#pragma once

#include <memory>

#include "linker/Linker.h"

namespace pcc {

// Builds a GenericLinker (see GenericLinker.h) configured for the
// 8080: `kI8080BaseAddress`/`kI8080CoffMachine` (see
// ../codegen/i8080/I8080Assembler.h), `kI8080RelocAbs16` patched via
// the shared `RelocKinds::patchAbs16`, and a synthesized entry object
// (`_start`: set up the real hardware stack, `call fn_main`, read the
// result into `hl`, halt) assembled exactly like any other object -
// `call fn_main` becomes a genuine external relocation, resolved
// exactly like any other cross-object call - see Z80LinkerFactory.h's
// own header comment for the original version of this same pattern.
//
// This same entry object is also the *only* place
// `ac`/`bc`/`cx`/`dx`/`ptr` (this backend's own shared pseudo-
// registers - see I8080Codegen.cpp's own `generate()` comment) are
// actually defined - every user object only ever *references* them,
// deferred as an ordinary external relocation the same way a call to
// a function defined in another file is.
std::unique_ptr<TargetLinker> makeI8080Linker();

}  // namespace pcc
