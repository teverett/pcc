#include "fileformat/atari2600/Atari2600RomFormat.h"

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

// A program based at $F000 (4K cartridge) pads to exactly 4096 bytes with $FF, then the three
// vectors (NMI/RESET/IRQ, all pointing at $F000) occupy the last 6 bytes.
void testRoundTrip4K() {
    rcc::ObjectFile obj = makeObject(0xF000, 300);
    rcc::Atari2600RomFormatWriter writer;
    std::string bytes = writer.write(obj);
    require(bytes.size() == 4096, "a 4K cartridge image must be exactly 4096 bytes");

    for (size_t i = 300; i < 4096 - 6; i++) {
        require(static_cast<uint8_t>(bytes[i]) == 0xFF, "unused space should be padded with $FF");
    }
    for (int v = 0; v < 3; v++) {
        size_t off = 4096 - 6 + static_cast<size_t>(v) * 2;
        uint32_t vec = static_cast<uint8_t>(bytes[off]) | (static_cast<uint8_t>(bytes[off + 1]) << 8);
        require(vec == 0xF000, "every one of NMI/RESET/IRQ should point at the program's own base");
    }

    rcc::Atari2600RomFormatReader reader;
    rcc::ObjectFile back = reader.read(bytes);
    require(back.base == 0xF000, "round trip lost the RESET vector's own base address");
    require(back.textBytes.size() == 4096, "reader should return the whole padded image, not trim it");
}

// A program based at $F800 (2K cartridge) pads to exactly 2048 bytes instead.
void testRoundTrip2K() {
    rcc::ObjectFile obj = makeObject(0xF800, 10);
    rcc::Atari2600RomFormatWriter writer;
    std::string bytes = writer.write(obj);
    require(bytes.size() == 2048, "a 2K cartridge image must be exactly 2048 bytes");

    rcc::Atari2600RomFormatReader reader;
    rcc::ObjectFile back = reader.read(bytes);
    require(back.base == 0xF800, "round trip lost the RESET vector's own base address");
}

void writerRejects(uint32_t base, size_t textSize, const char *why) {
    rcc::ObjectFile obj = makeObject(base, textSize);
    rcc::Atari2600RomFormatWriter writer;
    try {
        writer.write(obj);
    } catch (const std::runtime_error &) {
        return;
    }
    throw std::runtime_error(std::string("should have rejected: ") + why);
}

void testWriterRejections() {
    writerRejects(0x1000, 10, "a base address other than $F000/$F800");
    writerRejects(0xF000, 4096, "a program with no room left for the 6-byte vector table");
}

void readerRejects(const std::string &bytes, const char *why) {
    rcc::Atari2600RomFormatReader reader;
    try {
        reader.read(bytes);
    } catch (const std::runtime_error &) {
        return;
    }
    throw std::runtime_error(std::string("should have rejected: ") + why);
}

void testReaderRejections() {
    readerRejects(std::string(4095, '\0'), "a size that isn't 2048 or 4096 bytes");
    readerRejects(std::string(100, '\0'), "a tiny input far short of either cartridge size");
}

}  // namespace

int main() {
    try {
        testRoundTrip4K();
        testRoundTrip2K();
        testWriterRejections();
        testReaderRejections();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
