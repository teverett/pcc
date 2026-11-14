#include "fileformat/sna/SnaFormat.h"

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

// A program based at $8000 (the plain z80 target's own default) produces a 27-byte header
// followed by a 49152-byte 48K RAM dump, with the header's SP pointing at the reserved
// $FFFE-$FFFF bootstrap cell, which itself holds obj.base.
void testRoundTrip() {
    rcc::ObjectFile obj = makeObject(0x8000, 300);
    rcc::SnaFormatWriter writer;
    std::string bytes = writer.write(obj);
    require(bytes.size() == 27 + 49152, "an .sna image must be exactly 27 + 49152 bytes");

    uint32_t sp = static_cast<uint8_t>(bytes[23]) | (static_cast<uint8_t>(bytes[24]) << 8);
    require(sp == 0xFFFE, "header SP should point at the reserved bootstrap cell ($FFFE)");

    size_t bootstrapOffset = 27 + (0xFFFE - 0x4000);
    uint32_t entry = static_cast<uint8_t>(bytes[bootstrapOffset]) |
                      (static_cast<uint8_t>(bytes[bootstrapOffset + 1]) << 8);
    require(entry == 0x8000, "the bootstrap cell should hold the program's own base address");

    // The program's own bytes should sit at offset (base - $4000) within the RAM dump.
    size_t programOffset = 27 + (0x8000 - 0x4000);
    require(static_cast<uint8_t>(bytes[programOffset]) == 0, "program's first byte should be copied verbatim");
    require(static_cast<uint8_t>(bytes[programOffset + 1]) == 1, "program's second byte should be copied verbatim");

    rcc::SnaFormatReader reader;
    rcc::ObjectFile back = reader.read(bytes);
    require(back.base == 0x8000, "round trip lost the bootstrap entry address");
    require(back.textBytes.size() == 49152, "reader should return the whole 48K RAM dump, not trim it");
}

void writerRejects(uint32_t base, size_t textSize, const char *why) {
    rcc::ObjectFile obj = makeObject(base, textSize);
    rcc::SnaFormatWriter writer;
    try {
        writer.write(obj);
    } catch (const std::runtime_error &) {
        return;
    }
    throw std::runtime_error(std::string("should have rejected: ") + why);
}

void testWriterRejections() {
    writerRejects(0x1000, 10, "a base address below $4000");
    writerRejects(0xFFFE, 10, "a base address at or past the reserved bootstrap cell");
    writerRejects(0x8000, 32767, "a program that runs into the reserved bootstrap cell");
}

void readerRejects(const std::string &bytes, const char *why) {
    rcc::SnaFormatReader reader;
    try {
        reader.read(bytes);
    } catch (const std::runtime_error &) {
        return;
    }
    throw std::runtime_error(std::string("should have rejected: ") + why);
}

void testReaderRejections() {
    readerRejects(std::string(27 + 49152 - 1, '\0'), "a size that isn't 27 + 49152 bytes");
    readerRejects(std::string(27 + 49152, '\0'), "a header SP of 0, which points outside the RAM dump");
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
