#pragma once

#include "fileformat/FileFormat.h"

namespace rcc {

// 512-byte x86 boot sector: `obj.textBytes` padded with zeros and signed
// `0xAA55` at the end, per BIOS convention. No symbol table. Trailing zero
// padding is indistinguishable from trailing zero data, so `read()` always
// returns the full 508 bytes before the signature; `write()` always
// re-pads/re-signs, so write(read(x)) reproduces the same image byte for
// byte even though the original unpadded length isn't recoverable.
class RawFormatWriter : public FileFormatWriter {
   public:
    std::string write(const ObjectFile &obj) const override;
};

class RawFormatReader : public FileFormatReader {
   public:
    ObjectFile read(const std::string &bytes) const override;
};

}  // namespace rcc
