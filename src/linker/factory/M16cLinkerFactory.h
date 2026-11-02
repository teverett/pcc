#pragma once

#include <memory>

#include "linker/Linker.h"

namespace pcc {

// Builds a GenericLinker (see GenericLinker.h) configured for the
// M16C: `kM16cBaseAddress`/`kM16cCoffMachine` (see
// ../codegen/m16c/M16cAssembler.h), `kM16cRelocAbs16` patched via the
// shared `RelocKinds::patchAbs16` (the M16C dialect's own only
// relocation kind, little-endian), and a synthesized entry object
// (`_start`: set up `sp`, `jsr.w fn_main`, halt) assembled exactly
// like any other object - `jsr.w fn_main` becomes a genuine external
// relocation, resolved exactly like any other cross-object call.
//
// Unlike z80/m6502/m65c02, this backend has no shared memory-resident
// pseudo-registers (it uses the M16C's own real `R0`/`R1`/`A0`
// registers for all intermediate values - see M16cCodegen.h's own
// header comment), so the entry object here only needs the usual
// stack setup and call, nothing else.
std::unique_ptr<TargetLinker> makeM16cLinker();

}  // namespace pcc
