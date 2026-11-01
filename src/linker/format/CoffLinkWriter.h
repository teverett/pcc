#pragma once

#include "FormatWriter.h"

namespace pcc {

// `coff` - a fully-resolved COFF object (see ../coff/CoffWriter.h),
// no relocations at all (everything's already patched by this point -
// the same "whole program, nothing left unresolved" shape
// `--format=coff` on a *single* `.c` file has always produced, just
// reached via `--link` now instead). Unlike every other writer here,
// this one is genuinely architecture-generic: it packs each of
// `image`'s own address units into `ceil(unitBits/8)` big-endian
// bytes (3 for pdp7's 18-bit words, 1 for 8086's own bytes) and
// writes `image.machineType` straight into the COFF header, so it
// works for any LinkedImage's own unit width without needing to know
// which architecture produced it.
class CoffLinkWriter : public FormatWriter {
   public:
    std::string write(const LinkedImage &image) const override;
};

}  // namespace pcc
