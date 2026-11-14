#include "NesRomFormat.h"

#include <stdexcept>

namespace rcc {

namespace {
constexpr uint32_t kPrgBase = 0x8000;
constexpr size_t kHeaderSize = 16;
constexpr size_t kPrgSize = 32768;  // NROM-256: a full, non-mirrored $8000-$FFFF bank
constexpr size_t kVectorBytes = 6;  // NMI + RESET + IRQ, 2 bytes each
}  // namespace

std::string NesRomFormatWriter::write(const ObjectFile &obj) const {
    if (obj.base != kPrgBase) {
        throw std::runtime_error("NesRomFormat: base address must be $8000 (NROM's own fixed PRG-ROM window), "
                                  "got " +
                                  std::to_string(obj.base));
    }
    if (obj.textBytes.size() > kPrgSize - kVectorBytes) {
        throw std::runtime_error("NesRomFormat: program (" + std::to_string(obj.textBytes.size()) +
                                  " bytes) doesn't fit in a " + std::to_string(kPrgSize) +
                                  "-byte PRG-ROM bank once the 6-byte vector table is reserved - no "
                                  "bankswitching support");
    }

    std::string out;
    out.reserve(kHeaderSize + kPrgSize);
    out.push_back('N');
    out.push_back('E');
    out.push_back('S');
    out.push_back('\x1A');
    out.push_back(static_cast<char>(kPrgSize / 16384));  // PRG-ROM size, 16KB units
    out.push_back(static_cast<char>(0));                 // CHR-ROM size, 8KB units - 0 means CHR RAM
    out.push_back(static_cast<char>(0));                 // flags 6: mapper low nibble 0, horizontal mirroring
    out.push_back(static_cast<char>(0));                 // flags 7: mapper high nibble 0
    out.append(8, static_cast<char>(0));                 // reserved

    out.append(reinterpret_cast<const char *>(obj.textBytes.data()), obj.textBytes.size());
    out.append(kPrgSize - kVectorBytes - obj.textBytes.size(), static_cast<char>(0xFF));

    // NMI, RESET, IRQ - all three point at the same entry point (see this format's own header
    // comment for why).
    for (int i = 0; i < 3; i++) {
        out.push_back(static_cast<char>(obj.base & 0xFF));
        out.push_back(static_cast<char>((obj.base >> 8) & 0xFF));
    }
    return out;
}

ObjectFile NesRomFormatReader::read(const std::string &bytes) const {
    if (bytes.size() != kHeaderSize + kPrgSize) {
        throw std::runtime_error("NesRomFormat: " + std::to_string(bytes.size()) + " bytes isn't " +
                                  std::to_string(kHeaderSize + kPrgSize) +
                                  " bytes (16-byte iNES header + a 32768-byte PRG-ROM bank)");
    }
    if (bytes[0] != 'N' || bytes[1] != 'E' || bytes[2] != 'S' || bytes[3] != '\x1A') {
        throw std::runtime_error("NesRomFormat: missing iNES signature (\"NES\\x1A\") at offset 0");
    }

    size_t prgStart = kHeaderSize;
    size_t resetOffset = prgStart + kPrgSize - 4;  // past NMI, before IRQ
    ObjectFile obj;
    obj.base = static_cast<uint8_t>(bytes[resetOffset]) | (static_cast<uint8_t>(bytes[resetOffset + 1]) << 8);
    obj.textBytes.assign(bytes.begin() + static_cast<long>(prgStart), bytes.end());
    return obj;
}

}  // namespace rcc
