#pragma once

#include "fileformat/FileFormat.h"

namespace pcc {

// `--link --format=rim` - pdp7's bootstrap-loadable paper tape
// format: the same byte packing as `ptr/PtrFormat.h` (see
// ../Pdp7TapePacking.h) plus one trailing signature word real
// bootstrap hardware uses to find where to start execution. Needs an
// 18-bit-word (`unitBits == 18`) linked ObjectFile (pdp7's own).
//
// `read()` is the real inverse of `write()`: it recovers both the
// program's own words and (from the trailing signature word) the
// base address execution starts at, into `ObjectFile::base` - no
// information is lost, unlike flat/FlatFormat.h's/raw/RawFormat.h's
// own symbol-table-dropping stance (a rim tape never carried a symbol
// table to begin with, so there's nothing extra to lose here).
class RimFormatWriter : public FileFormatWriter {
   public:
    std::string write(const ObjectFile &obj) const override;
};

class RimFormatReader : public FileFormatReader {
   public:
    ObjectFile read(const std::string &bytes) const override;
};

}  // namespace pcc
