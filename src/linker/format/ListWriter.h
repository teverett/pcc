#pragma once

#include "FormatWriter.h"

namespace pcc {

// `.list` - a plain octal `name  address` symbol-table dump, sorted
// by name. Needs an 18-bit-word LinkedImage (pdp7's own).
class ListWriter : public FormatWriter {
   public:
    std::string write(const LinkedImage &image) const override;
};

}  // namespace pcc
