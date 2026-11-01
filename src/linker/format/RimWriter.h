#pragma once

#include "FormatWriter.h"

namespace pcc {

// `.rim` - pdp7's bootstrap-loadable paper tape format: the same
// byte packing as `.ptr` (see Pdp7TapePacking.h) plus one trailing
// signature word real bootstrap hardware uses to find where to
// start execution. Needs an 18-bit-word LinkedImage (pdp7's own).
class RimWriter : public FormatWriter {
   public:
    std::string write(const LinkedImage &image) const override;
};

}  // namespace pcc
