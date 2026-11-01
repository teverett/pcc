#pragma once

#include "FormatWriter.h"

namespace pcc {

// `aout` - a classic Unix `a.out`-style executable: the historical
// V7 Unix `struct exec` header (8 fields, 16-bit each - see
// AoutWriter.cpp's own comment for the exact layout and magic
// number) followed immediately by the program's own bytes. Like
// CoffLinkWriter, this one is genuinely architecture-generic: it
// packs `image`'s own address units into bytes the same way (see
// LinkedImage.h's `unitBits`) regardless of which architecture built
// it - fitting, since the real a.out format long predates any
// architecture-specific successor (COFF, ELF) and was itself reused
// across many different machines' own early Unix ports.
class AoutWriter : public FormatWriter {
   public:
    std::string write(const LinkedImage &image) const override;
};

}  // namespace pcc
