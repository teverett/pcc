#include "fileformat/amsdos/AmsdosFormat.h"

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

uint16_t u16(const std::string &bytes, size_t offset) {
    return static_cast<uint8_t>(bytes[offset]) | (static_cast<uint8_t>(bytes[offset + 1]) << 8);
}

// A program based at $4000 produces a 128-byte header with every field this writer sets
// matching the real AMSDOS layout, followed by its own bytes verbatim.
void testHeaderFields() {
    rcc::ObjectFile obj = makeObject(0x4000, 300);
    rcc::AmsdosFormatWriter writer;
    std::string bytes = writer.write(obj);
    require(bytes.size() == 128 + 300, "expected a 128-byte header plus the 300-byte program");

    require(static_cast<uint8_t>(bytes[0]) == 0, "user number should be 0");
    require(bytes.substr(1, 8) == "RCC     ", "filename field should be \"RCC     \"");
    require(bytes.substr(9, 3) == "BIN", "extension field should be \"BIN\"");
    for (size_t i = 12; i < 16; i++) require(bytes[i] == 0, "bytes 12-15 should be zero");
    require(static_cast<uint8_t>(bytes[18]) == 2, "file type should be 2 (unprotected binary)");
    require(u16(bytes, 19) == 300, "data length should be 300");
    require(u16(bytes, 21) == 0x4000, "data location should be the program's own base");
    require(static_cast<uint8_t>(bytes[23]) == 0xFF, "first block should be $FF");
    require(u16(bytes, 24) == 300, "logical length should be 300");
    require(u16(bytes, 26) == 0x4000, "entry address should be the program's own base");
    require(static_cast<uint8_t>(bytes[64]) == (300 & 0xFF) &&
                static_cast<uint8_t>(bytes[65]) == ((300 >> 8) & 0xFF) && static_cast<uint8_t>(bytes[66]) == 0,
            "real length (24-bit LE) should be 300");

    uint32_t sum = 0;
    for (size_t i = 0; i < 67; i++) sum += static_cast<uint8_t>(bytes[i]);
    require(u16(bytes, 67) == static_cast<uint16_t>(sum & 0xFFFF), "checksum should be the sum of bytes 0-66");

    for (size_t i = 69; i < 128; i++) require(bytes[i] == 0, "bytes 69-127 should be zero");
    require(bytes.substr(128) == std::string(reinterpret_cast<const char *>(obj.textBytes.data()),
                                              obj.textBytes.size()),
            "the program's own bytes should follow the header verbatim");
}

// Round trip: the reader recovers the base address (from the entry-address field) and the
// program's own bytes exactly.
void testRoundTrip() {
    rcc::ObjectFile obj = makeObject(0x8000, 42);
    rcc::AmsdosFormatWriter writer;
    std::string bytes = writer.write(obj);

    rcc::AmsdosFormatReader reader;
    rcc::ObjectFile back = reader.read(bytes);
    require(back.base == 0x8000, "round trip lost the entry address");
    require(back.textBytes.size() == 42, "round trip lost the program's own length");
    require(back.textBytes == obj.textBytes, "round trip corrupted the program's own bytes");
}

// Boundary: an empty program still produces a valid, checksum-correct header.
void testEmptyProgram() {
    rcc::ObjectFile obj = makeObject(0x4000, 0);
    rcc::AmsdosFormatWriter writer;
    std::string bytes = writer.write(obj);
    require(bytes.size() == 128, "an empty program should produce just the 128-byte header");
    rcc::AmsdosFormatReader reader;
    rcc::ObjectFile back = reader.read(bytes);
    require(back.textBytes.empty(), "round trip of an empty program should stay empty");
}

void writerRejects(size_t textSize, const char *why) {
    rcc::ObjectFile obj = makeObject(0x4000, textSize);
    rcc::AmsdosFormatWriter writer;
    try {
        writer.write(obj);
    } catch (const std::runtime_error &) {
        return;
    }
    throw std::runtime_error(std::string("should have rejected: ") + why);
}

// Boundary: exactly 65535 bytes is accepted (fits the 16-bit length fields); 65536 is rejected.
void testWriterBoundary() {
    rcc::ObjectFile atLimit = makeObject(0x4000, 65535);
    rcc::AmsdosFormatWriter writer;
    writer.write(atLimit);  // must not throw
    writerRejects(65536, "a program one byte over the 16-bit length fields' own limit");
}

void readerRejects(const std::string &bytes, const char *why) {
    rcc::AmsdosFormatReader reader;
    try {
        reader.read(bytes);
    } catch (const std::runtime_error &) {
        return;
    }
    throw std::runtime_error(std::string("should have rejected: ") + why);
}

void testReaderRejections() {
    readerRejects(std::string(127, '\0'), "a file shorter than the 128-byte header");
    // 128 zero bytes: checksum field (0) matches the sum of 67 zero bytes (0), so this
    // particular all-zero input is actually a valid (degenerate) header - use a corrupted,
    // non-matching checksum instead to exercise the real rejection path.
    std::string corrupted(128, '\0');
    corrupted[67] = 1;  // checksum field now disagrees with the all-zero sum
    readerRejects(corrupted, "a checksum that doesn't match bytes 0-66's own sum");
}

}  // namespace

int main() {
    try {
        testHeaderFields();
        testRoundTrip();
        testEmptyProgram();
        testWriterBoundary();
        testReaderRejections();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
