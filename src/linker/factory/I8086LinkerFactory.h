#pragma once

#include <memory>

#include "linker/Linker.h"

namespace pcc {

// Builds a GenericLinker (see GenericLinker.h) configured for the
// 8086/8088 backend (see src/main.cpp's own makeBackend() comment for
// why both targets share this one linker too): `kI8086BaseAddress`/
// `kI8086CoffMachine` (see ../../codegen/x86_8086/I8086Assembler.h),
// `kI8086RelocAbs16`/`kI8086RelocRel16` patched via the shared
// `RelocKinds::patchAbs16`/`patchRel16` (this target is plain byte-
// addressed little-endian real-mode x86 - no local patch function of
// its own is needed, unlike a target with a split hi/lo address load
// or a non-contiguous branch field), and a synthesized entry object
// (zero the segment registers, set up the stack at `0x7C00`, `call
// fn_main`, halt) assembled exactly like any other object - `call
// fn_main` becomes a genuine external relocation, resolved exactly
// like any other cross-object call, same pattern every other
// GenericLinker-based target's own factory already documents (see
// e.g. M68kLinkerFactory.h's own header comment).
std::unique_ptr<TargetLinker> makeI8086Linker();

}  // namespace pcc
