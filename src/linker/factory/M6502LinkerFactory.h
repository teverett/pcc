#pragma once

#include <memory>

#include "linker/Linker.h"

namespace pcc {

// Builds a GenericLinker (see GenericLinker.h) configured for the
// 6502: `kM6502BaseAddress`/`kM6502CoffMachine` (see
// ../codegen/m6502/M6502Assembler.h), its own three relocation kinds
// (`kM6502RelocAbs16` plus the low/high-byte-only `...Lo`/`...Hi` -
// see that same header's comment on why a 6502 needs three where z80
// only needs one), and a synthesized entry object (`_start`:
// `jsr fn_main`, read the result into `a`, halt) assembled exactly
// like any other object - `jsr fn_main` becomes a genuine external
// relocation, resolved exactly like any other cross-object call.
//
// Unlike Z80Linker.h's own entry object (which is the *only* place
// that shared pseudo-register storage is actually allocated - see
// that file's own header comment), this backend's own
// `ptr`/`ac`/`bc`/`cx`/`dx` are plain compile-time equates (see
// M6502Codegen.cpp's own `generate()` comment), not real storage -
// every object, including this entry one, binds them identically and
// locally, so none of them ever needs a cross-object relocation or a
// linker-visible symbol at all.
std::unique_ptr<TargetLinker> makeM6502Linker();

}  // namespace pcc
