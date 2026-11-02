#pragma once

#include <memory>

#include "linker/Linker.h"

namespace pcc {

// Builds a GenericLinker (see GenericLinker.h) configured for the
// stm32 (Cortex-M3/Thumb-2) backend: `kSTM32BaseAddress`/
// `kSTM32CoffMachine` (see ../../codegen/stm32/STM32Assembler.h),
// this dialect's own four relocation kinds (`Abs32` reused from the
// shared `RelocKinds::patchAbs32` for `dlt`'s own vector-table entry;
// `MovWLo`/`MovTHi` for `lea`'s own split 32-bit address load and
// `Branch25` for `bl`/`b`'s own wide Thumb-2 branch field, both
// patched by local functions in this file's own .cpp, not the shared
// RelocKinds, same precedent as ArmLinkerFactory.cpp's own split), and
// a synthesized entry object that replicates the real Cortex-M reset
// vector table STM32Codegen.cpp's own `--format=raw` path used to
// emit per-compile (initial-SP word, reset-handler word with its
// Thumb bit forced on, then `_start: bl fn_main` + a self-branch halt)
// - `bl fn_main` becomes a genuine external relocation, resolved
// exactly like any other cross-object call, same pattern
// M68kLinkerFactory.h's own header comment documents.
//
// This backend uses real CPU registers (r0-r2/sp/lr - see
// STM32Codegen.h's own header comment), so no shared-pseudo-register
// fixup is needed here at all.
//
// RAM is deliberately left out of this synthesized entry object
// entirely: unlike flash (laid out sequentially by GenericLinker's
// own `merge()`, same as every other migrated target), this
// backend's own RAM-resident globals are placed at the real, fixed
// hardware base `kRamBase` (STM32Codegen.cpp) by STM32Assembler.cpp
// itself, independently of any object's own flash placement - see
// that file's own `assembleSTM32Object` comment for exactly what that
// does and doesn't make correct across more than one linked object.
std::unique_ptr<TargetLinker> makeSTM32Linker();

}  // namespace pcc
