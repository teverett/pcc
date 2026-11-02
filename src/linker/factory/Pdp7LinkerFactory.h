#pragma once

#include <memory>

#include "linker/Linker.h"

namespace pcc {

// Builds a GenericLinker (see GenericLinker.h) configured for the
// pdp7 backend - the one target whose own `GenericLinkerConfig` sets
// `addressUnitBytes = 3`/`outputUnitBits = 18` (see that struct's own
// comments): every object's own `.textBytes` is already pdp7's real
// 18-bit words packed 3 bytes each, big-endian (see
// ../../codegen/pdp7/Pdp7Assembler.cpp's own `buildObjectFile()`), so
// GenericLinker's own byte-sequential layout and relocation-site
// indexing need no change at all - only the three local patch
// functions below (`kPdp7RelocOr`/`Add`/`Sub` - see
// ../../codegen/pdp7/Pdp7Assembler.h's own comment on why none of
// these are additive/relative byte patches, so none reuse
// GenericLinker.h's own shared `RelocKinds`) and the final packed-
// byte-to-word-address conversion (entirely GenericLinker's own job,
// driven by this config) are pdp7-specific at all.
//
// `kPdp7BaseAddress` (a word address) is multiplied by 3 here before
// being handed to `GenericLinkerConfig::baseAddress`, which expects
// that same packed-byte scale every object's own `.textBytes` size
// already uses - see that field's own comment for why.
//
// The synthesized entry object (`jms FN_main` then `hlt`) is
// assembled exactly like any other object - `jms FN_main` becomes a
// genuine external relocation (the default OR-combine - see
// Pdp7Assembler.cpp's own two-pass assembler), resolved exactly like
// any other cross-object reference, same pattern every other
// GenericLinker-based target's own factory already documents (see
// e.g. M68kLinkerFactory.h's own header comment) - pdp7's own real
// 8192-word (020000 octal) direct-addressing field width (see
// Pdp7Assembler.cpp's own `controlAddr`-equivalent encoding) is
// enforced inside this factory's own `patchPdp7Or` instead of as a
// separate pre-flight check GenericLinker's own shared merge() has no
// natural place for: every address reference in this dialect is
// always a relocation (even a same-file one, against the reserved
// pseudo-symbol `".."` - see GenericLinker.h's own header comment),
// so a per-relocation bounds check at patch time catches exactly the
// same real violations the old pre-flight total-size check did.
std::unique_ptr<TargetLinker> makePdp7Linker();

}  // namespace pcc
