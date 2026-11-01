#pragma once

#include <string>
#include <vector>

#include "LinkedImage.h"
#include "coff/CoffReader.h"

namespace pcc {

// The architecture-specific half of `--link` (see main.cpp): combines
// several independently-compiled COFF object files (see
// ../coff/CoffReader.h) - each produced by `pcc --format=coff` from
// its own single `.c` file - into one fully-resolved LinkedImage (see
// LinkedImage.h). Writing that image out in a particular format is a
// separate, decoupled step - see FormatWriter.h - so this interface
// only ever does "validate -> lay out sequentially -> resolve every
// relocation -> patch -> synthesize the entry sequence", never any
// output-format-specific byte packing. One implementation per target
// - Pdp7Linker.h/I8086Linker.h - the same split Codegen.h already
// uses for codegen: each target's own word size (pdp7's 18-bit words,
// addressed/relocated in units of 3 packed bytes - see
// Pdp7Assembler.cpp; 8086's plain byte addressing) and relocation-type
// semantics (pdp7's OR/`+`/`-` word-combine kinds; 8086's
// Abs16/Rel16) are different enough that there's no useful shared
// "generic merge" step to factor out above this interface.
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
    virtual LinkedImage merge(const std::vector<CoffObject> &objects) const = 0;

    // Which --format values are valid for `--link` with this target -
    // each one naming a FormatWriter (see FormatWriter.h) whose own
    // `unitBits` expectation matches what merge() above produces.
    virtual std::vector<std::string> outputFormats() const = 0;
};

}  // namespace pcc
