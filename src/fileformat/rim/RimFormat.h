#pragma once

#include "fileformat/FileFormat.h"

namespace pcc {

// pdp7 bootstrap paper tape: same packing as ptr/PtrFormat.h (see
// ../Pdp7TapePacking.h) plus a trailing signature word giving the execution
// start address, which `read()` recovers into `ObjectFile::base`.
class RimFormatWriter : public FileFormatWriter {
   public:
    std::string write(const ObjectFile &obj) const override;
};

class RimFormatReader : public FileFormatReader {
   public:
    ObjectFile read(const std::string &bytes) const override;
};

}  // namespace pcc
