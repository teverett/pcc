#include "Atari2600RomFormat.h"

#include <stdexcept>

namespace rcc {

namespace {
constexpr uint32_t k4KBase = 0xF000;
constexpr uint32_t k2KBase = 0xF800;
constexpr size_t k4KSize = 4096;
constexpr size_t k2KSize = 2048;
constexpr size_t kVectorBytes = 6;  // NMI + RESET + IRQ, 2 bytes each

size_t cartSizeForBase(uint32_t base) {
    if (base == k4KBase) return k4KSize;
    if (base == k2KBase) return k2KSize;
    throw std::runtime_error("Atari2600RomFormat: base address must be $F000 (4K cartridge) or $F800 (2K "
                              "cartridge), got " +
                              std::to_string(base));
}
}  // namespace

std::string Atari2600RomFormatWriter::write(const ObjectFile &obj) const {
    size_t cartSize = cartSizeForBase(obj.base);
    if (obj.textBytes.size() > cartSize - kVectorBytes) {
        throw std::runtime_error("Atari2600RomFormat: program (" + std::to_string(obj.textBytes.size()) +
                                  " bytes) doesn't fit in a " + std::to_string(cartSize) +
                                  "-byte cartridge once the 6-byte vector table is reserved - no bankswitching "
                                  "support");
    }
    std::string out;
    out.reserve(cartSize);
    out.append(reinterpret_cast<const char *>(obj.textBytes.data()), obj.textBytes.size());
    out.append(cartSize - kVectorBytes - obj.textBytes.size(), static_cast<char>(0xFF));

    // NMI, RESET, IRQ - all three point at the same entry point (see this format's own header
    // comment for why).
    for (int i = 0; i < 3; i++) {
        out.push_back(static_cast<char>(obj.base & 0xFF));
        out.push_back(static_cast<char>((obj.base >> 8) & 0xFF));
    }
    return out;
}

ObjectFile Atari2600RomFormatReader::read(const std::string &bytes) const {
    if (bytes.size() != k4KSize && bytes.size() != k2KSize) {
        throw std::runtime_error("Atari2600RomFormat: " + std::to_string(bytes.size()) +
                                  " bytes isn't a standard cartridge size (2048/4096)");
    }
    size_t resetOffset = bytes.size() - 4;  // past NMI, before IRQ
    ObjectFile obj;
    obj.base = static_cast<uint8_t>(bytes[resetOffset]) | (static_cast<uint8_t>(bytes[resetOffset + 1]) << 8);
    obj.textBytes.assign(bytes.begin(), bytes.end());
    return obj;
}

}  // namespace rcc
