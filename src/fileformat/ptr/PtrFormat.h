#pragma once

#include "fileformat/FileFormat.h"

namespace rcc {

// Same paper-tape packing as rim/RimFormat.h (see ../Pdp7TapePacking.h) but
// with no trailing bootstrap signature word, so no base address; `read()`
// leaves `ObjectFile::base` at 0.
class PtrFormatWriter : public FileFormatWriter {
   public:
    std::string write(const ObjectFile &obj) const override;
};

class PtrFormatReader : public FileFormatReader {
   public:
    ObjectFile read(const std::string &bytes) const override;
};

}  // namespace rcc
