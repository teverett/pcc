#pragma once

#include <memory>

#include "linker/Linker.h"

namespace pcc {

// Builds a GenericLinker (see GenericLinker.h) configured for the
// i386 backend: `kI386LoadAddr`/`kI386CoffMachine` (see
// ../../codegen/i386/I386Assembler.h), `kI386RelocAbs32` patched via
// the shared `RelocKinds::patchAbs32` and `kI386RelocRel32` patched
// via the shared `RelocKinds::patchRel32` (both already exist in
// GenericLinker.h - this dialect needed no new relocation-kind
// infrastructure at all), and a synthesized entry object replicating
// this backend's own whole-program Multiboot header + PIC-masking
// sequence verbatim (see ../../codegen/i386/I386Codegen.cpp's own
// `generate()`), followed by `call fn_main` (now a genuine external
// relocation, resolved exactly like any other cross-object call) and
// a halt loop.
//
// Unlike Z80LinkerFactory.h's own entry object, this backend uses
// real CPU registers (eax/ebx/ecx/edx - see I386Codegen.h's own
// header comment), so no shared-pseudo-register fixup is needed here
// at all - the only reason this entry object exists is to own the
// Multiboot header (which must be the very first thing in the linked
// image) and the PIC-masking sequence (which must run before any
// user code that might call `_ion()`).
std::unique_ptr<TargetLinker> makeI386Linker();

}  // namespace pcc
