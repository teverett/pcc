#pragma once

#include <string>
#include <vector>

#include "fileformat/ObjectFile.h"

namespace rcc {

// The architecture-specific half of `--link`: combines independently-compiled object files into
// one fully-resolved, linked ObjectFile; writing the result out in a particular file format is a
// separate step (see FileFormat.h). Every target shares one implementation, GenericLinker (see
// GenericLinker.h), configured per target rather than needing a bespoke subclass: GenericLinker's
// layout and relocation indexing are purely byte-based, so a word-addressed target like pdp7 just
// supplies a scale factor (GenericLinkerConfig::addressUnitBytes) and its own non-additive patch
// functions.
class TargetLinker {
   public:
    virtual ~TargetLinker() = default;

    // Throws std::runtime_error on a machine-type mismatch, an undefined symbol, or a name
    // defined by more than one object under this target's cross-file-visible naming convention;
    // any other same-named duplicate (a local temp, a backend-internal synthetic helper) is
    // tolerated silently, first one wins.
    //
    // `gcSections`: when true, each user object's own cross-file-visible definitions
    // (GenericLinker's own `isGlobalName` convention) split its `.text` into sections at their
    // boundaries, and only the sections transitively reachable from the entry sequence (always
    // kept in full) are included in the output - everything else is dropped, the same way a real
    // linker's `--gc-sections` drops unreferenced functions. Default false keeps today's
    // whole-object layout.
    virtual ObjectFile merge(const std::vector<ObjectFile> &objects, bool gcSections = false) const = 0;

    // --format values valid for `--link` with this target.
    virtual std::vector<std::string> outputFormats() const = 0;
};

}  // namespace rcc
