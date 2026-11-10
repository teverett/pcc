#include "codegen/arm64/Arm64Assembler.h"
#include "fileformat/macho/MachOFormat.h"

#include <iostream>
#include <stdexcept>

namespace {

uint32_t read32(const std::string &bytes, size_t offset) {
    uint32_t result = 0;
    for (size_t i = 0; i < 4; ++i) result |= static_cast<uint32_t>(static_cast<uint8_t>(bytes.at(offset + i))) << (8 * i);
    return result;
}

void checkBssSymbol(bool initialized) {
    rcc::ObjectFile object;
    object.machineType = rcc::kArm64DarwinMachine;
    object.textBytes = {0xc0, 0x03, 0x5f, 0xd6};
    if (initialized) object.dataBytes = {23, 0, 0, 0};
    object.dataSize = 8;
    object.symbols = {{"_zero", 0, true, true, true}};
    auto bytes = rcc::MachOFormatWriter().write(object);
    size_t command = 32;
    uint32_t symbolOffset = 0;
    unsigned sections = 0, bssSection = 0;
    for (uint32_t i = 0; i < read32(bytes, 16); ++i) {
        auto kind = read32(bytes, command);
        if (kind == 0x19) {
            auto count = read32(bytes, command + 64);
            for (uint32_t n = 0; n < count; ++n) {
                ++sections;
                if (bytes.compare(command + 72 + n * 80, 5, "__bss") == 0) bssSection = sections;
            }
        } else if (kind == 2) symbolOffset = read32(bytes, command + 8);
        command += read32(bytes, command + 4);
    }
    if (!symbolOffset || !bssSection || static_cast<uint8_t>(bytes.at(symbolOffset + 5)) != bssSection) {
        throw std::runtime_error("BSS symbol does not refer to its actual section");
    }
    if (bssSection != (initialized ? 3u : 2u)) throw std::runtime_error("incorrect BSS section ordering");
}

}

int main() {
    try {
        checkBssSymbol(false);
        checkBssSymbol(true);
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
