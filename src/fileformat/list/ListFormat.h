#pragma once

#include "fileformat/FileFormat.h"

namespace pcc {

// `--link --format=list` - a plain octal `name  address` symbol-table
// dump, sorted by name. Needs an 18-bit-word (`unitBits == 18`)
// linked ObjectFile (pdp7's own).
//
// `read()` always throws: this is a human-readable disassembly/
// symbol-table dump with no defined way to parse back into a real
// ObjectFile (there's no program text in it at all, just names and
// addresses) - unlike every other format in fileformat/, this one
// was never meant to round-trip.
class ListFormatWriter : public FileFormatWriter {
   public:
    std::string write(const ObjectFile &obj) const override;
};

class ListFormatReader : public FileFormatReader {
   public:
    ObjectFile read(const std::string &bytes) const override;
};

}  // namespace pcc
