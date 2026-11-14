#include "AmsdosFormat.h"

#include <stdexcept>

namespace rcc {

namespace {
constexpr size_t kHeaderSize = 128;
constexpr size_t kChecksummedBytes = 67;  // bytes 0-66
constexpr size_t kMaxProgramSize = 65535;

void putU16(std::string &out, size_t offset, uint16_t value) {
    out[offset] = static_cast<char>(value & 0xFF);
    out[offset + 1] = static_cast<char>((value >> 8) & 0xFF);
}

uint16_t getU16(const std::string &bytes, size_t offset) {
    return static_cast<uint8_t>(bytes[offset]) | (static_cast<uint8_t>(bytes[offset + 1]) << 8);
}
}  // namespace

std::string AmsdosFormatWriter::write(const ObjectFile &obj) const {
    if (obj.textBytes.size() > kMaxProgramSize) {
        throw std::runtime_error("AmsdosFormat: program (" + std::to_string(obj.textBytes.size()) +
                                  " bytes) is bigger than the AMSDOS header's own 16-bit length fields "
                                  "can hold (65535 bytes)");
    }

    std::string header(kHeaderSize, static_cast<char>(0));
    header[0] = 0;  // user number
    header.replace(1, 8, "RCC     ");
    header.replace(9, 3, "BIN");
    header[18] = 2;  // file type: unprotected binary, version 0
    putU16(header, 19, static_cast<uint16_t>(obj.textBytes.size()));  // data length
    putU16(header, 21, static_cast<uint16_t>(obj.base));              // data location
    header[23] = static_cast<char>(0xFF);                             // first block
    putU16(header, 24, static_cast<uint16_t>(obj.textBytes.size()));  // logical length
    putU16(header, 26, static_cast<uint16_t>(obj.base));              // entry address
    // real length: 24-bit LE, zero-extended from the same 16-bit size.
    uint32_t realLength = obj.textBytes.size();
    header[64] = static_cast<char>(realLength & 0xFF);
    header[65] = static_cast<char>((realLength >> 8) & 0xFF);
    header[66] = static_cast<char>((realLength >> 16) & 0xFF);

    uint32_t sum = 0;
    for (size_t i = 0; i < kChecksummedBytes; i++) sum += static_cast<uint8_t>(header[i]);
    putU16(header, 67, static_cast<uint16_t>(sum & 0xFFFF));

    std::string out;
    out.reserve(kHeaderSize + obj.textBytes.size());
    out.append(header);
    out.append(reinterpret_cast<const char *>(obj.textBytes.data()), obj.textBytes.size());
    return out;
}

ObjectFile AmsdosFormatReader::read(const std::string &bytes) const {
    if (bytes.size() < kHeaderSize) {
        throw std::runtime_error("AmsdosFormat: " + std::to_string(bytes.size()) +
                                  " bytes is smaller than the 128-byte AMSDOS header");
    }
    uint32_t sum = 0;
    for (size_t i = 0; i < kChecksummedBytes; i++) sum += static_cast<uint8_t>(bytes[i]);
    uint16_t storedChecksum = getU16(bytes, 67);
    if (static_cast<uint16_t>(sum & 0xFFFF) != storedChecksum) {
        throw std::runtime_error("AmsdosFormat: checksum mismatch - bytes 0-66 don't sum to the value "
                                  "stored at offset 67-68, so this isn't a header this writer produced");
    }

    ObjectFile obj;
    obj.base = getU16(bytes, 26);  // entry address
    obj.textBytes.assign(bytes.begin() + static_cast<long>(kHeaderSize), bytes.end());
    return obj;
}

}  // namespace rcc
