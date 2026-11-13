#include "Dos33Format.h"

#include <stdexcept>

namespace rcc {

std::string Dos33FormatWriter::write(const ObjectFile &obj) const {
    if (obj.textBytes.size() > 0xFFFF) {
        throw std::runtime_error("Dos33Format: program (" + std::to_string(obj.textBytes.size()) +
                                  " bytes) doesn't fit in a 16-bit DOS 3.3 binary file length");
    }
    std::string out;
    out.reserve(4 + obj.textBytes.size());
    out.push_back(static_cast<char>(obj.base & 0xFF));
    out.push_back(static_cast<char>((obj.base >> 8) & 0xFF));
    uint32_t len = static_cast<uint32_t>(obj.textBytes.size());
    out.push_back(static_cast<char>(len & 0xFF));
    out.push_back(static_cast<char>((len >> 8) & 0xFF));
    out.append(reinterpret_cast<const char *>(obj.textBytes.data()), obj.textBytes.size());
    return out;
}

ObjectFile Dos33FormatReader::read(const std::string &bytes) const {
    if (bytes.size() < 4) {
        throw std::runtime_error("Dos33Format: input is shorter than the 4-byte address/length header");
    }
    uint32_t loadAddr = static_cast<uint8_t>(bytes[0]) | (static_cast<uint8_t>(bytes[1]) << 8);
    uint32_t len = static_cast<uint8_t>(bytes[2]) | (static_cast<uint8_t>(bytes[3]) << 8);
    if (bytes.size() - 4 != len) {
        throw std::runtime_error("Dos33Format: header length (" + std::to_string(len) +
                                  ") doesn't match the actual payload (" + std::to_string(bytes.size() - 4) +
                                  " bytes)");
    }
    ObjectFile obj;
    obj.base = loadAddr;
    obj.textBytes.assign(bytes.begin() + 4, bytes.end());
    return obj;
}

}  // namespace rcc
