#pragma once

#include "fileformat/FileFormat.h"

namespace pcc {

// Plain octal `address: word` text listing, one line per word, for an
// 18-bit-word linked ObjectFile. Addresses must increase by exactly 1 per
// line; `read()` recovers `ObjectFile::base` from the first line's address.
class A7outFormatWriter : public FileFormatWriter {
   public:
    std::string write(const ObjectFile &obj) const override;
};

class A7outFormatReader : public FileFormatReader {
   public:
    ObjectFile read(const std::string &bytes) const override;
};

}  // namespace pcc
