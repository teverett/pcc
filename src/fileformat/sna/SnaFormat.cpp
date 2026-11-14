#include "SnaFormat.h"

#include <algorithm>
#include <stdexcept>

namespace rcc {

namespace {
constexpr uint32_t kRamBase = 0x4000;
constexpr size_t kRamSize = 49152;  // 48KB: the whole $4000-$FFFF window
constexpr size_t kHeaderSize = 27;
// Last 2 bytes of RAM: reserved for the bootstrap-trick entry address (see
// this format's own header comment).
constexpr uint32_t kBootstrapSlot = 0xFFFE;
}  // namespace

std::string SnaFormatWriter::write(const ObjectFile &obj) const {
    if (obj.base < kRamBase || obj.base >= kBootstrapSlot) {
        throw std::runtime_error("SnaFormat: base address must be within $4000-$FFFD (48K RAM, "
                                  "below the reserved $FFFE-$FFFF bootstrap cell), got " +
                                  std::to_string(obj.base));
    }
    if (obj.base + obj.textBytes.size() > kBootstrapSlot) {
        throw std::runtime_error("SnaFormat: program (" + std::to_string(obj.textBytes.size()) +
                                  " bytes loaded at $" + std::to_string(obj.base) +
                                  ") runs into the reserved $FFFE-$FFFF bootstrap cell - no room left "
                                  "in 48K RAM");
    }

    std::string header(kHeaderSize, static_cast<char>(0));
    header[25] = 1;  // interrupt mode 1 - the real ROM's own post-boot default
    header[26] = 7;  // border color white - the real ROM's own post-boot default
    // SP: points at the bootstrap cell below, so a loader's own bare RET lands on obj.base.
    header[23] = static_cast<char>(kBootstrapSlot & 0xFF);
    header[24] = static_cast<char>((kBootstrapSlot >> 8) & 0xFF);

    std::vector<uint8_t> ram(kRamSize, 0);
    std::copy(obj.textBytes.begin(), obj.textBytes.end(), ram.begin() + (obj.base - kRamBase));
    size_t bootstrapOffset = kBootstrapSlot - kRamBase;
    ram[bootstrapOffset] = static_cast<uint8_t>(obj.base & 0xFF);
    ram[bootstrapOffset + 1] = static_cast<uint8_t>((obj.base >> 8) & 0xFF);

    std::string out;
    out.reserve(kHeaderSize + kRamSize);
    out.append(header);
    out.append(reinterpret_cast<const char *>(ram.data()), ram.size());
    return out;
}

ObjectFile SnaFormatReader::read(const std::string &bytes) const {
    if (bytes.size() != kHeaderSize + kRamSize) {
        throw std::runtime_error("SnaFormat: " + std::to_string(bytes.size()) + " bytes isn't " +
                                  std::to_string(kHeaderSize + kRamSize) +
                                  " bytes (27-byte header + a 49152-byte 48K RAM dump)");
    }
    uint32_t sp = static_cast<uint8_t>(bytes[23]) | (static_cast<uint8_t>(bytes[24]) << 8);
    if (sp < kRamBase) {
        throw std::runtime_error("SnaFormat: header SP ($" + std::to_string(sp) +
                                  ") doesn't point inside the 48K RAM dump - not a bootstrap-trick "
                                  "snapshot this writer produced");
    }
    size_t bootstrapOffset = kHeaderSize + (sp - kRamBase);
    if (bootstrapOffset + 1 >= bytes.size()) {
        throw std::runtime_error("SnaFormat: header SP ($" + std::to_string(sp) +
                                  ") leaves no room for the 2-byte bootstrap entry address");
    }

    ObjectFile obj;
    obj.base = static_cast<uint8_t>(bytes[bootstrapOffset]) |
               (static_cast<uint8_t>(bytes[bootstrapOffset + 1]) << 8);
    obj.textBytes.assign(bytes.begin() + static_cast<long>(kHeaderSize), bytes.end());
    return obj;
}

}  // namespace rcc
