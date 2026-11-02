#include "FlatWriter.h"

namespace pcc {

std::string FlatWriter::write(const LinkedImage &image) const {
    uint32_t bytesPerUnit = (image.unitBits + 7) / 8;
    std::string out;
    out.reserve(image.words.size() * bytesPerUnit);
    for (uint32_t w : image.words) {
        for (uint32_t b = bytesPerUnit; b-- > 0;) {
            out.push_back(static_cast<char>((w >> (8 * b)) & 0xFF));
        }
    }
    return out;
}

}  // namespace pcc
