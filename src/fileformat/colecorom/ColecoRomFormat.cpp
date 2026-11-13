#include "ColecoRomFormat.h"

#include <array>
#include <cstdint>
#include <stdexcept>

namespace rcc {

namespace {
constexpr std::array<size_t, 4> kStandardSizes = {8192, 16384, 24576, 32768};

bool hasSignature(const std::string &bytes) {
    if (bytes.size() < 2) return false;
    auto b0 = static_cast<uint8_t>(bytes[0]);
    auto b1 = static_cast<uint8_t>(bytes[1]);
    return (b0 == 0x55 && b1 == 0xAA) || (b0 == 0xAA && b1 == 0x55);
}
}  // namespace

std::string ColecoRomFormatWriter::write(const ObjectFile &obj) const {
    std::string image(reinterpret_cast<const char *>(obj.textBytes.data()), obj.textBytes.size());
    if (!hasSignature(image)) {
        throw std::runtime_error(
            "ColecoRomFormat: program doesn't start with the cartridge signature (0x55AA/0xAA55) - "
            "link against --target=z80-coleco, not plain z80");
    }

    size_t padded = 0;
    for (size_t s : kStandardSizes) {
        if (image.size() <= s) {
            padded = s;
            break;
        }
    }
    if (padded == 0) {
        throw std::runtime_error("ColecoRomFormat: program (" + std::to_string(image.size()) +
                                  " bytes) is bigger than the largest standard cartridge size (32768 bytes)");
    }

    image.append(padded - image.size(), static_cast<char>(0xFF));
    return image;
}

ObjectFile ColecoRomFormatReader::read(const std::string &bytes) const {
    if (!hasSignature(bytes)) {
        throw std::runtime_error("ColecoRomFormat: missing cartridge signature (0x55AA/0xAA55) at offset 0");
    }
    bool knownSize = false;
    for (size_t s : kStandardSizes) {
        if (bytes.size() == s) knownSize = true;
    }
    if (!knownSize) {
        throw std::runtime_error("ColecoRomFormat: " + std::to_string(bytes.size()) +
                                  " bytes isn't one of the standard cartridge sizes (8192/16384/24576/32768)");
    }

    ObjectFile obj;
    obj.base = 0x8000;
    obj.textBytes.assign(bytes.begin(), bytes.end());
    return obj;
}

}  // namespace rcc
