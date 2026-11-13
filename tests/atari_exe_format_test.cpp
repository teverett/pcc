#include "fileformat/atari/AtariExeFormat.h"

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

// The writer emits a leading $FFFF marker, one segment covering the program (little-endian
// start/end, end inclusive), then a second 2-byte segment poking RUNAD ($02E0-$02E1) with the
// program's own base address.
void testRoundTrip() {
    rcc::ObjectFile obj = makeObject(0x2000, 300);
    rcc::AtariExeFormatWriter writer;
    std::string bytes = writer.write(obj);

    std::string expected;
    expected.push_back(static_cast<char>(0xFF));
    expected.push_back(static_cast<char>(0xFF));
    expected.push_back(0x00);
    expected.push_back(0x20);  // start 0x2000
    expected.push_back(0x2B);
    expected.push_back(0x21);  // end 0x212B (0x2000 + 300 - 1)
    expected.append(reinterpret_cast<const char *>(obj.textBytes.data()), obj.textBytes.size());
    expected.push_back(static_cast<char>(0xE0));
    expected.push_back(0x02);  // RUNAD lo = $02E0
    expected.push_back(static_cast<char>(0xE1));
    expected.push_back(0x02);  // RUNAD hi = $02E1
    expected.push_back(0x00);
    expected.push_back(0x20);  // poked value: base 0x2000, little-endian
    require(bytes == expected, "round trip's byte layout doesn't match the expected program+RUNAD segments");

    // The reader only ever reconstructs the first segment - the RUNAD poke is parsed (to find
    // where it ends) but intentionally discarded, since ObjectFile can't represent more than one
    // (base, bytes) pair.
    rcc::AtariExeFormatReader reader;
    rcc::ObjectFile back = reader.read(bytes);
    require(back.base == obj.base, "round trip lost the program's load address");
    require(back.textBytes == obj.textBytes, "round trip lost the program bytes");
}

void rejects(const std::string &bytes, const char *why) {
    rcc::AtariExeFormatReader reader;
    try {
        reader.read(bytes);
    } catch (const std::runtime_error &) {
        return;
    }
    throw std::runtime_error(std::string("should have rejected: ") + why);
}

void testMalformedInput() {
    // Missing the leading $FFFF marker.
    rejects(std::string("\x00\x00\x00\x20\x00\x20", 6), "missing $FFFF marker");

    // Truncated first-segment header.
    rejects(std::string("\xFF\xFF\x00\x20", 4), "truncated segment header");

    // A segment whose declared length runs past the end of the file.
    std::string truncatedData;
    truncatedData.push_back(static_cast<char>(0xFF));
    truncatedData.push_back(static_cast<char>(0xFF));
    truncatedData.push_back(0x00);
    truncatedData.push_back(0x20);  // start 0x2000
    truncatedData.push_back(0x0F);
    truncatedData.push_back(0x20);  // end 0x200F (claims 16 bytes)
    truncatedData.push_back('a');   // only 1 byte actually present
    rejects(truncatedData, "segment data running past end of file");

    // An end address before the start address.
    std::string backwards;
    backwards.push_back(static_cast<char>(0xFF));
    backwards.push_back(static_cast<char>(0xFF));
    backwards.push_back(0x00);
    backwards.push_back(0x20);  // start 0x2000
    backwards.push_back(0xFF);
    backwards.push_back(0x1F);  // end 0x1FFF - before start
    rejects(backwards, "end address before start address");
}

}  // namespace

int main() {
    try {
        testRoundTrip();
        testMalformedInput();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
