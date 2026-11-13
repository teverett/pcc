#include "PrgFormat.h"

#include <stdexcept>

namespace rcc {

std::string PrgFormatWriter::write(const ObjectFile &obj) const {
    if (obj.base > 0xFFFF) {
        throw std::runtime_error("PrgFormat: load address " + std::to_string(obj.base) +
                                  " doesn't fit in a PRG file's 16-bit header");
    }
    std::string out;
    out.reserve(2 + obj.textBytes.size());
    out.push_back(static_cast<char>(obj.base & 0xFF));
    out.push_back(static_cast<char>((obj.base >> 8) & 0xFF));
    out.append(reinterpret_cast<const char *>(obj.textBytes.data()), obj.textBytes.size());
    return out;
}

ObjectFile PrgFormatReader::read(const std::string &bytes) const {
    if (bytes.size() < 2) {
        throw std::runtime_error("PrgFormat: input is shorter than the 2-byte load-address header");
    }
    ObjectFile obj;
    obj.base = static_cast<uint8_t>(bytes[0]) | (static_cast<uint8_t>(bytes[1]) << 8);
    obj.textBytes.assign(bytes.begin() + 2, bytes.end());
    return obj;
}

}  // namespace rcc
