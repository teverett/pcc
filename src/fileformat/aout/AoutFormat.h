#pragma once

#include "fileformat/FileFormat.h"

namespace rcc {

// Classic V7/BSD a.out: 32-byte OMAGIC `struct exec` header, `struct
// relocation_info` relocations, `struct nlist` symbols, string table with a
// leading 4-byte total-size field (self-inclusive).
// `a_data`/`a_bss` are always 0 since every backend interleaves code and
// data into one flat `.text`. `relocation_info` has no bit field for
// relocation type, so it's packed into the 2-bit `r_length` field;
// `write()` throws if a type doesn't fit in 2 bits. BSD `r_baserel`/
// `r_jmptable`/`r_relative` bits are unused and left 0.
class AoutFormatWriter : public FileFormatWriter {
   public:
    std::string write(const ObjectFile &obj) const override;
};

class AoutFormatReader : public FileFormatReader {
   public:
    ObjectFile read(const std::string &bytes) const override;
};

}  // namespace rcc
