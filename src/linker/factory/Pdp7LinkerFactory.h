#pragma once

#include <cstdint>
#include <memory>

#include "codegen/pdp7/Pdp7Assembler.h"
#include "linker/Linker.h"

namespace rcc {

// Builds a GenericLinker for the PDP-7, whose 18-bit words are packed
// 3 bytes each, big-endian; `addressUnitBytes = 3`/`outputUnitBits = 18`
// tell GenericLinker this packing, and `baseAddressWords` (a word
// address, default `kPdp7BaseAddress`) is scaled by 3 to match. The
// three local patchers (kPdp7RelocOr/Add/Sub) exist because none of
// pdp7's relocation shapes are plain additive/relative byte patches.
// The 8192-word (020000 octal) direct-addressing limit is enforced
// per-relocation in patchPdp7Or/Add rather than as a pre-flight check,
// since every address reference here is a relocation.
//
// A non-default `baseAddressWords` links the given objects to run
// starting somewhere other than the ordinary user-program base - the
// real kernel's own low-memory-resident region (`.. = 0` in
// `../../../src-assem/sys/s1.s`) starts near word address 0, not
// `kPdp7BaseAddress` (4096) - needed to keep kernel-resident code
// (e.g. a disk-completion poll loop) outside whatever address range a
// swappable "user" region's own transfers might overwrite: a transfer
// spanning the entire ordinary 4096-8191 user range would otherwise
// overwrite the very poll loop waiting on it, hanging the transfer.
// The entry sequence is unchanged (`jms FN_main; hlt` plus the shared
// runtime) - this does not replicate the real interrupt vector
// table/CAL-trap entry that would really live at low addresses, only
// the base address itself. Literal 0 is NOT safe for code that makes
// real function calls: this runtime's own `__addr` scratch cell would
// land on word address 9 at that base, which is the real PDP-7's own
// hardware autoindex register 9, so using it as a scratch cell hangs
// real code - a small nonzero base (e.g. 16) avoids it and still
// matches how much low memory the real kernel's own interrupt vector
// table needs anyway.
std::unique_ptr<TargetLinker> makePdp7Linker(uint32_t baseAddressWords = kPdp7BaseAddress);

}  // namespace rcc
