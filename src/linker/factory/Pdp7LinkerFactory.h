#pragma once

#include <memory>

#include "linker/Linker.h"

namespace pcc {

// Builds a GenericLinker for the PDP-7, whose 18-bit words are packed
// 3 bytes each, big-endian; `addressUnitBytes = 3`/`outputUnitBits = 18`
// tell GenericLinker this packing, and `kPdp7BaseAddress` (a word
// address) is scaled by 3 to match. The three local patchers
// (kPdp7RelocOr/Add/Sub) exist because none of pdp7's relocation
// shapes are plain additive/relative byte patches. The 8192-word
// (020000 octal) direct-addressing limit is enforced per-relocation in
// patchPdp7Or/Add rather than as a pre-flight check, since every
// address reference here is a relocation.
std::unique_ptr<TargetLinker> makePdp7Linker();

}  // namespace pcc
