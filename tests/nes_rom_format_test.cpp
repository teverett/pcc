#include "fileformat/nes/NesRomFormat.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}

rcc::ObjectFile makeObject(uint32_t base, size_t textSize) {
    rcc::ObjectFile obj;
    obj.base = base;
    obj.textBytes.resize(textSize);
    for (size_t i = 0; i < textSize; i++) obj.textBytes[i] = static_cast<uint8_t>(i & 0xFF);
    return obj;
}

// A program based at $8000 (NROM's only legal base) produces a 16-byte iNES header followed by
// a 32768-byte PRG-ROM bank, padded with $FF and ending in the NMI/RESET/IRQ vector table, all
// three pointing at $8000.
void testRoundTrip() {
    rcc::ObjectFile obj = makeObject(0x8000, 300);
    rcc::NesRomFormatWriter writer;
    std::string bytes = writer.write(obj);
    require(bytes.size() == 16 + 32768, "an iNES image must be exactly 16 + 32768 bytes");

    require(bytes[0] == 'N' && bytes[1] == 'E' && bytes[2] == 'S' && bytes[3] == '\x1A',
            "missing iNES signature");
    require(static_cast<uint8_t>(bytes[4]) == 2, "PRG-ROM size field should be 2 (32768 / 16384)");
    require(static_cast<uint8_t>(bytes[5]) == 0, "CHR-ROM size field should be 0 (CHR RAM)");

    for (size_t i = 16 + 300; i < 16 + 32768 - 6; i++) {
        require(static_cast<uint8_t>(bytes[i]) == 0xFF, "unused PRG-ROM space should be padded with $FF");
    }
    for (int v = 0; v < 3; v++) {
        size_t off = 16 + 32768 - 6 + static_cast<size_t>(v) * 2;
        uint32_t vec = static_cast<uint8_t>(bytes[off]) | (static_cast<uint8_t>(bytes[off + 1]) << 8);
        require(vec == 0x8000, "every one of NMI/RESET/IRQ should point at the program's own base");
    }

    rcc::NesRomFormatReader reader;
    rcc::ObjectFile back = reader.read(bytes);
    require(back.base == 0x8000, "round trip lost the RESET vector's own base address");
    require(back.textBytes.size() == 32768, "reader should return the whole PRG-ROM bank, not trim it");
}

void writerRejects(uint32_t base, size_t textSize, const char *why) {
    rcc::ObjectFile obj = makeObject(base, textSize);
    rcc::NesRomFormatWriter writer;
    try {
        writer.write(obj);
    } catch (const std::runtime_error &) {
        return;
    }
    throw std::runtime_error(std::string("should have rejected: ") + why);
}

void testWriterRejections() {
    writerRejects(0x1000, 10, "a base address other than $8000");
    writerRejects(0x8000, 32768, "a program with no room left for the 6-byte vector table");
}

void readerRejects(const std::string &bytes, const char *why) {
    rcc::NesRomFormatReader reader;
    try {
        reader.read(bytes);
    } catch (const std::runtime_error &) {
        return;
    }
    throw std::runtime_error(std::string("should have rejected: ") + why);
}

void testReaderRejections() {
    readerRejects(std::string(16 + 32768 - 1, '\0'), "a size that isn't 16 + 32768 bytes");
    std::string badSignature(16 + 32768, '\0');
    readerRejects(badSignature, "a missing iNES signature");
}

}  // namespace

int main() {
    try {
        testRoundTrip();
        testWriterRejections();
        testReaderRejections();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
