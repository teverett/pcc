#include "CmdFormat.h"

#include <algorithm>
#include <stdexcept>

namespace rcc {

namespace {
constexpr size_t kMaxLoadChunk = 256;
}  // namespace

std::string CmdFormatWriter::write(const ObjectFile &obj) const {
    if (obj.base > 0xFFFF || obj.base + obj.textBytes.size() > 0x10000) {
        throw std::runtime_error("CmdFormat: program (base=" + std::to_string(obj.base) + ", " +
                                  std::to_string(obj.textBytes.size()) +
                                  " bytes) doesn't fit in the Z80's 16-bit address space");
    }

    std::string out;
    out.reserve(obj.textBytes.size() + obj.textBytes.size() / kMaxLoadChunk * 4 + 8);

    size_t offset = 0;
    uint32_t addr = obj.base;
    while (offset < obj.textBytes.size()) {
        size_t chunk = std::min(kMaxLoadChunk, obj.textBytes.size() - offset);
        out.push_back(static_cast<char>(1));
        out.push_back(static_cast<char>((chunk + 2) & 0xFF));
        out.push_back(static_cast<char>(addr & 0xFF));
        out.push_back(static_cast<char>((addr >> 8) & 0xFF));
        out.append(reinterpret_cast<const char *>(obj.textBytes.data() + offset), chunk);
        offset += chunk;
        addr += static_cast<uint32_t>(chunk);
    }

    out.push_back(static_cast<char>(2));
    out.push_back(static_cast<char>(2));
    out.push_back(static_cast<char>(obj.base & 0xFF));
    out.push_back(static_cast<char>((obj.base >> 8) & 0xFF));
    return out;
}

ObjectFile CmdFormatReader::read(const std::string &bytes) const {
    ObjectFile obj;
    bool haveBase = false;
    bool haveTransfer = false;
    uint32_t nextAddr = 0;
    size_t i = 0;

    while (i < bytes.size()) {
        if (i + 2 > bytes.size()) {
            throw std::runtime_error("CmdFormat: truncated record (no room for a type/length byte pair)");
        }
        uint8_t type = static_cast<uint8_t>(bytes[i]);
        uint8_t rawLen = static_cast<uint8_t>(bytes[i + 1]);
        i += 2;

        if (type == 1) {
            // A 0/1/2 LENGTH byte always means 256/257/258 here - see CmdFormat.h's own header
            // comment for why a literal 0-2-byte data area can never legitimately occur.
            uint32_t dataArea = rawLen < 3 ? static_cast<uint32_t>(rawLen) + 256 : rawLen;
            if (dataArea < 2) {
                throw std::runtime_error("CmdFormat: type-1 record's data area is too small for a load address");
            }
            size_t codeLen = dataArea - 2;
            if (i + 2 + codeLen > bytes.size()) {
                throw std::runtime_error("CmdFormat: type-1 record runs past the end of the file");
            }
            uint32_t addr = static_cast<uint8_t>(bytes[i]) | (static_cast<uint8_t>(bytes[i + 1]) << 8);
            i += 2;
            if (!haveBase) {
                obj.base = addr;
                haveBase = true;
                nextAddr = addr;
            } else if (addr != nextAddr) {
                throw std::runtime_error("CmdFormat: load blocks aren't contiguous - expected address " +
                                          std::to_string(nextAddr) + ", got " + std::to_string(addr));
            }
            obj.textBytes.insert(obj.textBytes.end(), bytes.begin() + static_cast<long>(i),
                                  bytes.begin() + static_cast<long>(i + codeLen));
            i += codeLen;
            nextAddr = addr + static_cast<uint32_t>(codeLen);
        } else if (type == 2) {
            if (rawLen != 2 || i + 2 > bytes.size()) {
                throw std::runtime_error("CmdFormat: malformed type-2 (transfer address) record");
            }
            i += 2;
            haveTransfer = true;
        } else {
            throw std::runtime_error("CmdFormat: unsupported record type " + std::to_string(type));
        }
    }

    if (!haveTransfer) {
        throw std::runtime_error("CmdFormat: file has no type-2 transfer-address record");
    }
    return obj;
}

}  // namespace rcc
