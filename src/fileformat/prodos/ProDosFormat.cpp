#include "ProDosFormat.h"

#include <stdexcept>

namespace rcc {

std::string ProDosFormatWriter::write(const ObjectFile &obj) const {
    if (obj.base > 0xFFFF) {
        throw std::runtime_error("ProDosFormat: load address " + std::to_string(obj.base) +
                                  " doesn't fit in ProDOS's 16-bit aux_type");
    }
    std::string out;
    out.reserve(obj.textBytes.size() + 3);
    out.append(reinterpret_cast<const char *>(obj.textBytes.data()), obj.textBytes.size());
    out.push_back(static_cast<char>(kProDosFileTypeBin));
    out.push_back(static_cast<char>(obj.base & 0xFF));
    out.push_back(static_cast<char>((obj.base >> 8) & 0xFF));
    return out;
}

ObjectFile ProDosFormatReader::read(const std::string &bytes) const {
    if (bytes.size() < 3) {
        throw std::runtime_error("ProDosFormat: input is shorter than the 3-byte file_type/aux_type footer");
    }
    size_t dataLen = bytes.size() - 3;
    ObjectFile obj;
    obj.textBytes.assign(bytes.begin(), bytes.begin() + static_cast<long>(dataLen));
    obj.base = static_cast<uint8_t>(bytes[dataLen + 1]) | (static_cast<uint8_t>(bytes[dataLen + 2]) << 8);
    return obj;
}

}  // namespace rcc
