#pragma once

#include <string>

#include "linker/LinkedImage.h"

namespace pcc {

// Writes one final `--link` output format from an already fully-
// linked LinkedImage (see that file) - the half of linking that's
// purely "how do these final bytes get serialized", decoupled from
// *how* the image got resolved (see Linker.h's TargetLinker, the
// other half). One implementation per format - CoffLinkWriter,
// RimWriter, PtrWriter, A7outWriter, ListWriter, BinWriter - each
// free-standing: nothing here ties a writer to one particular
// architecture. Wiring a writer up to a *new* architecture later is
// just adding its name to that architecture's own
// TargetLinker::outputFormats() and main.cpp's `makeFormatWriter()`,
// as long as that architecture's own merge() produces a LinkedImage
// in the shape the writer expects (its own `unitBits` check, if it
// has one, throws a clear error otherwise) - see LinkedImage.h.
class FormatWriter {
   public:
    virtual ~FormatWriter() = default;

    // Throws std::runtime_error on anything that makes `image`
    // unwritable in this format (wrong `unitBits`, a program too big
    // for a fixed-size container, an unresolvable entry symbol - that
    // last one's actually caught earlier, by TargetLinker::merge()
    // itself, since every format needs it).
    virtual std::string write(const LinkedImage &image) const = 0;
};

}  // namespace pcc
