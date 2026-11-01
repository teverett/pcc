#include "BinWriter.h"

#include <stdexcept>

namespace pcc {

std::string BinWriter::write(const LinkedImage &image) const {
    if (image.unitBits != 8) {
        throw std::runtime_error("BinWriter: expected a byte-addressed image, got unitBits=" +
                                  std::to_string(image.unitBits));
    }
    constexpr size_t kSectorSize = 512;
    if (image.words.size() > kSectorSize - 2) {
        throw std::runtime_error("linked program (" + std::to_string(image.words.size()) +
                                  " bytes) doesn't fit in one " + std::to_string(kSectorSize) + "-byte boot sector");
    }
    std::string out;
    out.reserve(kSectorSize);
    for (uint32_t w : image.words) out.push_back(static_cast<char>(w & 0xFF));
    out.resize(kSectorSize - 2, '\0');
    out.push_back(static_cast<char>(0x55));
    out.push_back(static_cast<char>(0xAA));
    return out;
}

}  // namespace pcc
