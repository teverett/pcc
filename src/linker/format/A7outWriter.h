#pragma once

#include "FormatWriter.h"

namespace pcc {

// `.a7out` - a plain octal `address: word` text listing, one line
// per word. Needs an 18-bit-word LinkedImage (pdp7's own).
class A7outWriter : public FormatWriter {
   public:
    std::string write(const LinkedImage &image) const override;
};

}  // namespace pcc
