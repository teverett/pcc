#include "RawFormat.h"

#include <stdexcept>

namespace rcc {

namespace {
constexpr size_t kSectorSize = 512;
}  // namespace

std::string RawFormatWriter::write(const ObjectFile &obj) const {
    if (obj.unitBits != 8) {
        throw std::runtime_error("RawFormat: expected a byte-addressed image, got unitBits=" +
                                  std::to_string(obj.unitBits));
    }
    if (obj.textBytes.size() > kSectorSize - 2) {
        throw std::runtime_error("linked program (" + std::to_string(obj.textBytes.size()) +
                                  " bytes) doesn't fit in one " + std::to_string(kSectorSize) + "-byte boot sector");
    }
    std::string out;
    out.reserve(kSectorSize);
    out.append(reinterpret_cast<const char *>(obj.textBytes.data()), obj.textBytes.size());
    out.resize(kSectorSize - 2, '\0');
    out.push_back(static_cast<char>(0x55));
    out.push_back(static_cast<char>(0xAA));
    return out;
}

ObjectFile RawFormatReader::read(const std::string &bytes) const {
    if (bytes.size() != kSectorSize) {
        throw std::runtime_error("RawFormat: expected a " + std::to_string(kSectorSize) +
                                  "-byte boot sector, got " + std::to_string(bytes.size()) + " bytes");
    }
    if (static_cast<uint8_t>(bytes[kSectorSize - 2]) != 0x55 || static_cast<uint8_t>(bytes[kSectorSize - 1]) != 0xAA) {
        throw std::runtime_error("RawFormat: missing the 0x55 0xAA boot-sector signature");
    }
    ObjectFile obj;
    obj.textBytes.assign(bytes.begin(), bytes.begin() + static_cast<long>(kSectorSize - 2));
    obj.unitBits = 8;
    return obj;
}

}  // namespace rcc
