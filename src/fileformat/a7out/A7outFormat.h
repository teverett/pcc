#pragma once

#include "fileformat/FileFormat.h"

namespace pcc {

// `--link --format=a7out` - a plain octal `address: word` text
// listing, one line per word. Needs an 18-bit-word (`unitBits == 18`)
// linked ObjectFile (pdp7's own).
//
// `read()` is the real inverse of `write()`: each line's own address
// must be exactly one more than the previous (this format never skips
// an address - every word, including padding, is always listed), so
// the first line's address recovers `ObjectFile::base` and every
// word's own value is read straight back. No symbol table existed in
// this format to begin with, so none is lost either (unlike
// list/ListFormat.h's own symbol-only listing, which has nothing
// else to lose from this format's own perspective).
class A7outFormatWriter : public FileFormatWriter {
   public:
    std::string write(const ObjectFile &obj) const override;
};

class A7outFormatReader : public FileFormatReader {
   public:
    ObjectFile read(const std::string &bytes) const override;
};

}  // namespace pcc
