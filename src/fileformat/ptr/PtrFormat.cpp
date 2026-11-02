#include "PtrFormat.h"

#include <stdexcept>

#include "fileformat/Pdp7TapePacking.h"

namespace pcc {

std::string PtrFormatWriter::write(const ObjectFile &obj) const {
    if (obj.unitBits != 18) {
        throw std::runtime_error("PtrFormat: expected an 18-bit-word image, got unitBits=" +
                                  std::to_string(obj.unitBits));
    }
    return packPdp7Tape(wordsFromPdp7TextBytes(obj.textBytes), obj.base, /*rimSignature=*/false);
}

ObjectFile PtrFormatReader::read(const std::string &bytes) const {
    ObjectFile obj;
    std::vector<uint32_t> words = unpackPdp7Tape(bytes, /*rimSignature=*/false);
    obj.textBytes = pdp7TextBytesFromWords(words);
    obj.unitBits = 18;
    return obj;
}

}  // namespace pcc
