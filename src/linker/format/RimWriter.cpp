#include "RimWriter.h"

#include <stdexcept>

#include "Pdp7TapePacking.h"

namespace pcc {

std::string RimWriter::write(const LinkedImage &image) const {
    if (image.unitBits != 18) {
        throw std::runtime_error("RimWriter: expected an 18-bit-word image, got unitBits=" +
                                  std::to_string(image.unitBits));
    }
    return packPdp7Tape(image, /*rimSignature=*/true);
}

}  // namespace pcc
