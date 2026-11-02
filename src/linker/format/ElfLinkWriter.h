#pragma once

#include "FormatWriter.h"

namespace pcc {

// `elf` - a minimal 32-bit ELF executable (see ../../elf/ElfWriter.h
// for the real writer this delegates to) wrapping an already fully-
// linked LinkedImage, the same role CoffLinkWriter/AoutWriter/
// RawWriter each already play for their own container. 8086-only
// today (see that header's own comment on `e_machine` for why) -
// requires `image.unitBits == 8`, same check as RawWriter.cpp's own.
class ElfLinkWriter : public FormatWriter {
   public:
    std::string write(const LinkedImage &image) const override;
};

}  // namespace pcc
