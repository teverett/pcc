#pragma once

#include <memory>

#include "linker/Linker.h"

namespace pcc {

// Builds a GenericLinker (see GenericLinker.h) configured for the
// Z80: `kZ80BaseAddress`/`kZ80CoffMachine` (see
// ../codegen/z80/Z80Assembler.h), `kZ80RelocAbs16` patched via the
// shared `RelocKinds::patchAbs16` (the Z80 dialect's own only
// relocation kind - see that same header's comment on why there's no
// Rel16 here the way 8086 needs one), and a synthesized entry object
// (`_start`: zero the stack, `call fn_main`, read the result into
// `hl`, halt) assembled exactly like any other object - `call
// fn_main` becomes a genuine external relocation, resolved exactly
// like any other cross-object call, same pattern
// I8086LinkerFactory.h's own header comment documents
// for 8086.
//
// This same entry object is also the *only* place
// `ac`/`_bc`/`cx`/`dx`/`ptr` (the Z80 backend's own shared pseudo-
// registers - see Z80Codegen.cpp's own `generate()` comment on why
// they can't be redeclared per object) are actually defined - every
// user object only ever *references* them, deferred as an ordinary
// external relocation the same way a call to a function defined in
// another file is.
std::unique_ptr<TargetLinker> makeZ80Linker();

}  // namespace pcc
