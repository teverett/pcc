#pragma once

#include <memory>

#include "linker/Linker.h"

namespace pcc {

// Builds a GenericLinker (see GenericLinker.h) configured for the
// STM8: `kSTM8BaseAddress`/`kSTM8CoffMachine` (see
// ../codegen/stm8/STM8Assembler.h), `kSTM8RelocAbs16` patched via the
// shared `RelocKinds::patchAbs16BE` (big-endian - the STM8 is a
// big-endian machine, see STM8Codegen.h's own header comment), and a
// synthesized entry object (`_start`: set up the stack, `call
// fn_main`, read the result into `x`, halt) assembled exactly like
// any other object - `call fn_main` becomes a genuine external
// relocation, resolved exactly like any other cross-object call,
// same pattern I8086LinkerFactory.h's own header comment
// documents for 8086.
//
// This same entry object is also the *only* place
// `ac`/`bc`/`cx`/`dx`/`ptr`/`lc` (the STM8 backend's own shared
// pseudo-registers - see STM8Codegen.cpp's own `generate()` comment
// on why they can't be redeclared per object) are actually defined -
// every user object only ever *references* them, deferred as an
// ordinary external relocation the same way a call to a function
// defined in another file is.
std::unique_ptr<TargetLinker> makeSTM8Linker();

}  // namespace pcc
