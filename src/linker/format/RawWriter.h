#pragma once

#include "FormatWriter.h"

namespace pcc {

// `--format=raw` - a flat, directly-bootable 512-byte boot sector:
// the image's own bytes, padded with zeros and signed `0xAA55` at
// the very end (the x86 BIOS's own boot-sector convention - real
// hardware/`qemu`/etc. all refuse to boot a floppy image missing it).
// Needs a byte-addressed LinkedImage (8086's own) that already fits
// in 510 bytes.
class RawWriter : public FormatWriter {
   public:
    std::string write(const LinkedImage &image) const override;
};

}  // namespace pcc
