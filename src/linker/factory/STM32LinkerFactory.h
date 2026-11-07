#pragma once

#include <memory>

#include "linker/Linker.h"

namespace pcc {

// Builds a GenericLinker for the stm32 (Cortex-M3/Thumb-2) backend: relocation kinds for `lea`'s
// split 32-bit address load (MovWLo/MovTHi) and `bl`/`b`'s wide Thumb-2 branch field (Branch25),
// and a synthesized entry object replicating the Cortex-M reset vector table (initial-SP word,
// reset-handler word with Thumb bit forced on, `_start: bl fn_main` + self-branch halt).
//
// RAM is left out of the entry object: RAM-resident globals are placed at the fixed hardware base
// `kRamBase` by STM32Assembler.cpp itself, independently of flash placement.
std::unique_ptr<TargetLinker> makeSTM32Linker();

}  // namespace pcc
