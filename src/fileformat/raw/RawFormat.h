#pragma once

#include "fileformat/FileFormat.h"

namespace pcc {

// `--link --format=raw` - a flat, directly-bootable 512-byte boot
// sector: `obj.textBytes` as-is, padded with zeros and signed
// `0xAA55` at the very end (the x86 BIOS's own boot-sector
// convention - real hardware/`qemu`/etc. all refuse to boot a floppy
// image missing it). Needs a byte-addressed (`unitBits == 8`) linked
// ObjectFile (8086's own) that already fits in 510 bytes - never a
// per-file `--format` to compile with (see FileFormatKind's own
// comment on why).
//
// Lossy by design, the same spirit as flat/FlatFormat.h's own stance:
// `write()` never carries any symbol table through (same reasoning -
// a boot sector has no room for one, and nothing ever reads one back
// out of real boot-sector hardware anyway), and `read()` can't
// recover the exact original (pre-padding) length either, since
// trailing zero padding and trailing zero *data* bytes are
// indistinguishable once written - it always returns the full 508
// bytes before the signature, verified bytes are padding or not. This
// is still a faithful round trip in the sense that matters: writing
// what `read()` returns reproduces the exact same 510-byte-padded,
// signed image, byte for byte, every time (`write()` always re-pads/
// re-signs regardless of its input's own trailing bytes).
class RawFormatWriter : public FileFormatWriter {
   public:
    std::string write(const ObjectFile &obj) const override;
};

class RawFormatReader : public FileFormatReader {
   public:
    ObjectFile read(const std::string &bytes) const override;
};

}  // namespace pcc
