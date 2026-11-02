#include "ElfLinkWriter.h"

#include <cstdint>
#include <stdexcept>
#include <vector>

#include "elf/ElfWriter.h"

namespace pcc {

std::string ElfLinkWriter::write(const LinkedImage &image) const {
    if (image.unitBits != 8) {
        throw std::runtime_error("ElfLinkWriter: expected a byte-addressed image, got unitBits=" +
                                  std::to_string(image.unitBits));
    }
    std::vector<uint8_t> bytes;
    bytes.reserve(image.words.size());
    for (uint32_t w : image.words) bytes.push_back(static_cast<uint8_t>(w & 0xFF));
    return writeElfExecutable(image.base, bytes);
}

}  // namespace pcc
