#include "RimFormat.h"

#include <stdexcept>

#include "fileformat/Pdp7TapePacking.h"

namespace rcc {

std::string RimFormatWriter::write(const ObjectFile &obj) const {
    if (obj.unitBits != 18) {
        throw std::runtime_error("RimFormat: expected an 18-bit-word image, got unitBits=" +
                                  std::to_string(obj.unitBits));
    }
    return packPdp7Tape(wordsFromPdp7TextBytes(obj.textBytes), obj.base, /*rimSignature=*/true);
}

ObjectFile RimFormatReader::read(const std::string &bytes) const {
    ObjectFile obj;
    uint32_t base = 0;
    std::vector<uint32_t> words = unpackPdp7Tape(bytes, /*rimSignature=*/true, &base);
    obj.textBytes = pdp7TextBytesFromWords(words);
    obj.unitBits = 18;
    obj.base = base;
    return obj;
}

}  // namespace rcc
