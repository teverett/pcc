#include "fileformat/prg/PrgFormat.h"

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

// A program round-trips to exactly a 2-byte little-endian load address followed by the bytes
// verbatim - no length field at all, unlike Dos33Format.
void testRoundTrip() {
    rcc::ObjectFile obj = makeObject(0x1001, 300);
    rcc::PrgFormatWriter writer;
    std::string bytes = writer.write(obj);

    std::string expected;
    expected.push_back(0x01);  // load address 0x1001, little-endian
    expected.push_back(0x10);
    expected.append(reinterpret_cast<const char *>(obj.textBytes.data()), obj.textBytes.size());
    require(bytes == expected, "round trip's byte layout doesn't match the expected 2-byte header plus payload");

    rcc::PrgFormatReader reader;
    rcc::ObjectFile back = reader.read(bytes);
    require(back.base == obj.base, "round trip lost the load address");
    require(back.textBytes == obj.textBytes, "round trip lost the program bytes");
}

// An empty program is still a valid 2-byte-header PRG file, unlike Dos33Format's EOF-block
// convention which needs no such special case.
void testEmptyProgram() {
    rcc::ObjectFile obj = makeObject(0x1201, 0);
    rcc::PrgFormatWriter writer;
    std::string bytes = writer.write(obj);
    require(bytes.size() == 2, "an empty program should still write the 2-byte header, nothing else");
    require(static_cast<uint8_t>(bytes[0]) == 0x01 && static_cast<uint8_t>(bytes[1]) == 0x12,
            "empty-program header has the wrong load address");

    rcc::PrgFormatReader reader;
    rcc::ObjectFile back = reader.read(bytes);
    require(back.base == obj.base, "empty-program round trip lost the load address");
    require(back.textBytes.empty(), "empty-program round trip should read back zero payload bytes");
}

void testWriterRejectsOversizedBase() {
    rcc::ObjectFile obj = makeObject(0x10000, 1);
    rcc::PrgFormatWriter writer;
    try {
        writer.write(obj);
    } catch (const std::runtime_error &) {
        return;
    }
    throw std::runtime_error("should have rejected a load address that doesn't fit in 16 bits");
}

void testReaderRejectsTruncatedHeader() {
    rcc::PrgFormatReader reader;
    try {
        reader.read(std::string(1, '\x00'));
    } catch (const std::runtime_error &) {
        return;
    }
    throw std::runtime_error("should have rejected an input shorter than the 2-byte header");
}

}  // namespace

int main() {
    try {
        testRoundTrip();
        testEmptyProgram();
        testWriterRejectsOversizedBase();
        testReaderRejectsTruncatedHeader();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
