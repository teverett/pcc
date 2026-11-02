#pragma once

#include <memory>

#include "linker/Linker.h"

namespace pcc {

// Builds a GenericLinker (see ../GenericLinker.h) configured for
// PIC18 (see ../../codegen/pic18/PIC18Codegen.h): `kPIC18BaseAddress`/
// `kPIC18CoffMachine`/`kPIC18DataBaseAddress` (see
// ../../codegen/pic18/PIC18Assembler.h), the three `kPIC18RelocGotoCall*`
// kinds patched via this file's own `patchGotoCallHiNibble`/`Mid`/`Lo`
// (a word-vs-byte shift plus a three-level carry-correct split - see
// each function's own comment), `kPIC18RelocDataAbs8` patched via
// this file's own `patchDataAbs8` (a plain single-byte additive patch
// with a real-hardware bound check), and a synthesized entry object
// (real reset-vector setup: clear `FSR0H`, point `SP` at `SSTACK`,
// `call fn_main`, halt) assembled exactly like any other object -
// `call fn_main` becomes a genuine external relocation, resolved
// exactly like any other cross-object call, same pattern
// Z80LinkerFactory.cpp's own header comment documents for z80.
//
// This entry object does *not* run any user object's own global
// variable initializers - it's built once, before it has ever seen
// any user object at all, so it can't know any of them exist. Each
// user object's own PIC18Codegen::generate() already arranges for the
// one defining `main` to run its own globals' init code as
// `fn_main`'s own prologue instead - see that file's own `objectMode`
// comment for why, and for the resulting restriction (every global a
// file references has to be defined in that same file).
std::unique_ptr<TargetLinker> makePIC18Linker();

}  // namespace pcc
