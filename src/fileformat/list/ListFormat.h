#pragma once

#include "fileformat/FileFormat.h"

namespace rcc {

// Plain octal `name  address` symbol-table dump, sorted by name.
// `read()` always throws: there's no program text, so this format cannot
// round-trip.
class ListFormatWriter : public FileFormatWriter {
   public:
    std::string write(const ObjectFile &obj) const override;
};

class ListFormatReader : public FileFormatReader {
   public:
    ObjectFile read(const std::string &bytes) const override;
};

}  // namespace rcc
