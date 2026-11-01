#pragma once

#include "FormatWriter.h"

namespace pcc {

// `.ptr` - the same paper-tape byte packing as `.rim` (see
// Pdp7TapePacking.h) but with no trailing bootstrap signature word -
// loading one still needs a separate "go" command telling the
// simulator/hardware where to start. Needs an 18-bit-word LinkedImage
// (pdp7's own).
class PtrWriter : public FormatWriter {
   public:
    std::string write(const LinkedImage &image) const override;
};

}  // namespace pcc
