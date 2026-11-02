#pragma once

#include <string>
#include <vector>

#include "LinkedImage.h"
#include "objectformat/ObjectFile.h"

namespace pcc {

// The architecture-specific half of `--link` (see main.cpp): combines
// several independently-compiled object files (see
// ../objectformat/ObjectFormat.h - any supported format, COFF/ELF/
// Mach-O/a.out/raw, read back into the same format-neutral
// `ObjectFile`) - each produced by `pcc --format=<fmt>` from its own
// single `.c` file - into one fully-resolved LinkedImage (see
// LinkedImage.h). Writing that image out in a particular format is a
// separate, decoupled step - see FormatWriter.h - so this interface
// only ever does "validate -> lay out sequentially -> resolve every
// relocation -> patch -> synthesize the entry sequence", never any
// output-format-specific byte packing. Every target in this project
// shares one single implementation, GenericLinker (see
// GenericLinker.h), configured per target via GenericLinkerConfig
// rather than needing its own bespoke subclass - including pdp7,
// whose own 18-bit words (addressed/relocated in units of 3 packed
// bytes - see Pdp7Assembler.cpp) and non-additive OR/`+`/`-` word-
// combine relocation semantics look like they'd need a bespoke
// implementation at first glance, but don't: GenericLinker's own
// sequential layout and relocation-site indexing are already purely
// byte-based internally (every `ObjSymbol`/`ObjRelocation` is always
// a plain byte offset regardless of target - see ObjectFile.h's own
// comments), so a target whose own real addressable unit packs more
// than one byte just tells GenericLinker that scale factor
// (`GenericLinkerConfig::addressUnitBytes`) and supplies its own
// local, non-additive patch functions (see GenericLinker.h's own
// `RelocKinds` comment on why a target with unusual combine semantics
// always needs local patch functions anyway, word-addressed or not) -
// see Pdp7LinkerFactory.cpp for the concrete instance of this.
class TargetLinker {
   public:
    virtual ~TargetLinker() = default;

    // Combines `objects`, in the order given (this project's own
    // sequential-layout convention - see each implementation's own
    // header comment), into one resolved LinkedImage. Throws
    // std::runtime_error on a machine-type mismatch, a symbol no
    // given object defines (including the entry symbol, `main`
    // itself), or a name defined by more than one object that
    // actually follows this target's own cross-file-visible naming
    // convention (see each implementation's own header comment for
    // exactly which prefixes that means) - every other same-named
    // duplicate (a local temp/literal label, a backend-internal
    // synthetic helper like pdp7's __one/__zero/__divmod, always
    // resolved purely locally within their own object's own two-pass
    // assembly and never via a cross-file relocation) is a harmless
    // duplicate, not a conflict, and tolerated silently.
    virtual LinkedImage merge(const std::vector<ObjectFile> &objects) const = 0;

    // Which --format values are valid for `--link` with this target -
    // each one naming a FormatWriter (see FormatWriter.h) whose own
    // `unitBits` expectation matches what merge() above produces.
    virtual std::vector<std::string> outputFormats() const = 0;
};

}  // namespace pcc
