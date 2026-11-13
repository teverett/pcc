#pragma once

#include <memory>

#include "linker/Linker.h"

namespace rcc {

// Builds a GenericLinker for `--target=z80-coleco`: same Z80 instruction
// set/relocations as `makeZ80Linker()` (see Z80LinkerFactory.h), but with
// a ColecoVision cartridge header - not a plain `ld sp,...`/`call fn_main`
// stub - as the synthesized entry object, since a cartridge's first bytes
// at $8000 must be the BIOS's own signature/pointer table, not arbitrary
// code.
std::unique_ptr<TargetLinker> makeColecoLinker();

}  // namespace rcc
