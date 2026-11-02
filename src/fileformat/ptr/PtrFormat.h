#pragma once

#include "fileformat/FileFormat.h"

namespace pcc {

// `--link --format=ptr` - the same paper-tape byte packing as
// `rim/RimFormat.h` (see ../Pdp7TapePacking.h) but with no trailing
// bootstrap signature word - loading one still needs a separate "go"
// command telling the simulator/hardware where to start. Needs an
// 18-bit-word (`unitBits == 18`) linked ObjectFile (pdp7's own).
//
// `read()` recovers the program's own words exactly, but - since a
// plain `ptr` tape never carried a base address at all (see above) -
// leaves `ObjectFile::base` at its default `0` rather than guessing;
// a caller that knows the real base out-of-band (the same "go"
// command a human operator would use) sets it separately.
class PtrFormatWriter : public FileFormatWriter {
   public:
    std::string write(const ObjectFile &obj) const override;
};

class PtrFormatReader : public FileFormatReader {
   public:
    ObjectFile read(const std::string &bytes) const override;
};

}  // namespace pcc
