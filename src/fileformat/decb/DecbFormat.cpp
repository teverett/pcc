#include "DecbFormat.h"

#include <stdexcept>

namespace rcc {

namespace {
constexpr uint8_t kDataBlock = 0x00;
constexpr uint8_t kEofBlock = 0xFF;

void pushBE16(std::string &out, uint32_t value) {
    out.push_back(static_cast<char>((value >> 8) & 0xFF));
    out.push_back(static_cast<char>(value & 0xFF));
}

uint32_t readBE16(const std::string &bytes, size_t at) { return (static_cast<uint8_t>(bytes[at]) << 8) | static_cast<uint8_t>(bytes[at + 1]); }
}  // namespace

std::string DecbFormatWriter::write(const ObjectFile &obj) const {
    if (obj.base > 0xFFFF || obj.base + obj.textBytes.size() > 0x10000) {
        throw std::runtime_error("DecbFormat: program (base=" + std::to_string(obj.base) + ", " +
                                  std::to_string(obj.textBytes.size()) +
                                  " bytes) doesn't fit in a 16-bit address space");
    }

    std::string out;
    out.reserve(obj.textBytes.size() + 11);

    out.push_back(static_cast<char>(kDataBlock));
    pushBE16(out, static_cast<uint32_t>(obj.textBytes.size()));
    pushBE16(out, obj.base);
    out.append(reinterpret_cast<const char *>(obj.textBytes.data()), obj.textBytes.size());

    out.push_back(static_cast<char>(kEofBlock));
    pushBE16(out, 0);
    pushBE16(out, obj.base);
    return out;
}

ObjectFile DecbFormatReader::read(const std::string &bytes) const {
    ObjectFile obj;
    bool haveBase = false;
    bool haveTransfer = false;
    uint32_t nextAddr = 0;
    size_t i = 0;

    while (i < bytes.size()) {
        if (i + 5 > bytes.size()) {
            throw std::runtime_error("DecbFormat: truncated block header (no room for type/length/address)");
        }
        uint8_t type = static_cast<uint8_t>(bytes[i]);
        uint32_t length = readBE16(bytes, i + 1);
        uint32_t addr = readBE16(bytes, i + 3);
        i += 5;

        if (type == kDataBlock) {
            if (i + length > bytes.size()) {
                throw std::runtime_error("DecbFormat: data block runs past the end of the file");
            }
            if (!haveBase) {
                obj.base = addr;
                haveBase = true;
                nextAddr = addr;
            } else if (addr != nextAddr) {
                throw std::runtime_error("DecbFormat: data blocks aren't contiguous - expected address " +
                                          std::to_string(nextAddr) + ", got " + std::to_string(addr));
            }
            obj.textBytes.insert(obj.textBytes.end(), bytes.begin() + static_cast<long>(i),
                                  bytes.begin() + static_cast<long>(i + length));
            i += length;
            nextAddr = addr + length;
        } else if (type == kEofBlock) {
            if (length != 0) {
                throw std::runtime_error("DecbFormat: malformed EOF block (length must be 0)");
            }
            haveTransfer = true;
        } else {
            throw std::runtime_error("DecbFormat: unsupported block type " + std::to_string(type));
        }
    }

    if (!haveTransfer) {
        throw std::runtime_error("DecbFormat: file has no EOF (transfer-address) block");
    }
    return obj;
}

}  // namespace rcc
