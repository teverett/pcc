#include "PtrWriter.h"

#include <stdexcept>

#include "Pdp7TapePacking.h"

namespace pcc {

std::string PtrWriter::write(const LinkedImage &image) const {
    if (image.unitBits != 18) {
        throw std::runtime_error("PtrWriter: expected an 18-bit-word image, got unitBits=" +
                                  std::to_string(image.unitBits));
    }
    return packPdp7Tape(image, /*rimSignature=*/false);
}

}  // namespace pcc
