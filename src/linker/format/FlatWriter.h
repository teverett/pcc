#pragma once

#include "FormatWriter.h"

namespace pcc {

// `flat` - exactly `image.words`, packed into `ceil(unitBits/8)`
// big-endian bytes per unit (same architecture-generic packing
// CoffLinkWriter.h's own header comment documents), nothing else at
// all: no padding, no signature, no fixed container size. This is
// the genuinely generic "--link --format=raw" every target that
// isn't 8086 actually wants - RawWriter.h's own `--format=raw` is
// specifically an x86 boot sector (padded to 510 bytes, signed
// `0x55 0xAA`, a hard 510-byte ceiling - real requirements for a
// floppy a real BIOS will boot, meaningless for any other target),
// so this is a separate writer rather than a second meaning
// overloaded onto that same name. A target whose own simulator/
// hardware just loads bytes at a fixed address directly (e.g. z80's
// own check_z80() - see Z80Linker.h) uses this one.
class FlatWriter : public FormatWriter {
   public:
    std::string write(const LinkedImage &image) const override;
};

}  // namespace pcc
