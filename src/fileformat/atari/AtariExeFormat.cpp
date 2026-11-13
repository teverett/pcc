#include "AtariExeFormat.h"

#include <stdexcept>

namespace rcc {

namespace {
// The OS's own RUNAD vector (run address) - see AtariExeFormat.h's own header comment for why
// this writer pokes it.
constexpr uint32_t kRunAdLo = 0x02E0;
constexpr uint32_t kRunAdHi = 0x02E1;

void pushLo16(std::string &out, uint32_t v) {
    out.push_back(static_cast<char>(v & 0xFF));
    out.push_back(static_cast<char>((v >> 8) & 0xFF));
}

void writeSegment(std::string &out, uint32_t start, uint32_t end, const uint8_t *data, size_t len) {
    pushLo16(out, start);
    pushLo16(out, end);
    out.append(reinterpret_cast<const char *>(data), len);
}
}  // namespace

std::string AtariExeFormatWriter::write(const ObjectFile &obj) const {
    uint32_t end = obj.base + (obj.textBytes.empty() ? 0 : static_cast<uint32_t>(obj.textBytes.size()) - 1);
    if (end > 0xFFFF) {
        throw std::runtime_error("AtariExeFormat: program (base " + std::to_string(obj.base) + ", " +
                                  std::to_string(obj.textBytes.size()) + " bytes) runs past a 16-bit address");
    }
    std::string out;
    out.push_back(static_cast<char>(0xFF));
    out.push_back(static_cast<char>(0xFF));
    writeSegment(out, obj.base, end, obj.textBytes.data(), obj.textBytes.size());

    uint8_t runAd[2] = {static_cast<uint8_t>(obj.base & 0xFF), static_cast<uint8_t>((obj.base >> 8) & 0xFF)};
    writeSegment(out, kRunAdLo, kRunAdHi, runAd, 2);
    return out;
}

ObjectFile AtariExeFormatReader::read(const std::string &bytes) const {
    if (bytes.size() < 2 || static_cast<uint8_t>(bytes[0]) != 0xFF || static_cast<uint8_t>(bytes[1]) != 0xFF) {
        throw std::runtime_error("AtariExeFormat: missing the leading $FFFF segment-file marker");
    }
    size_t pos = 2;
    if (pos + 4 > bytes.size()) {
        throw std::runtime_error("AtariExeFormat: input is shorter than the first segment's 4-byte header");
    }
    uint32_t start = static_cast<uint8_t>(bytes[pos]) | (static_cast<uint8_t>(bytes[pos + 1]) << 8);
    uint32_t end = static_cast<uint8_t>(bytes[pos + 2]) | (static_cast<uint8_t>(bytes[pos + 3]) << 8);
    pos += 4;
    if (end < start) {
        throw std::runtime_error("AtariExeFormat: first segment's end address is before its start address");
    }
    size_t len = end - start + 1;
    if (pos + len > bytes.size()) {
        throw std::runtime_error("AtariExeFormat: first segment's data runs past the end of the file");
    }
    ObjectFile obj;
    obj.base = start;
    obj.textBytes.assign(bytes.begin() + static_cast<long>(pos), bytes.begin() + static_cast<long>(pos + len));
    return obj;
}

}  // namespace rcc
