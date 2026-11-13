#include "fileformat/cmd/CmdFormat.h"

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

// A program whose .text fits one load block round-trips to exactly one type-1 record (LENGTH =
// code size + 2, no wraparound) followed by one type-2 transfer-address record pointing at base.
void testSingleChunkRoundTrip() {
    rcc::ObjectFile obj = makeObject(0x6000, 10);
    rcc::CmdFormatWriter writer;
    std::string bytes = writer.write(obj);

    std::string expected;
    expected.push_back(1);
    expected.push_back(12);  // 10 code bytes + 2 address bytes
    expected.push_back(0x00);
    expected.push_back(0x60);
    expected.append(reinterpret_cast<const char *>(obj.textBytes.data()), obj.textBytes.size());
    expected.push_back(2);
    expected.push_back(2);
    expected.push_back(0x00);
    expected.push_back(0x60);
    require(bytes == expected, "single-chunk byte layout doesn't match the expected TYPE/LENGTH/DATA records");

    rcc::CmdFormatReader reader;
    rcc::ObjectFile back = reader.read(bytes);
    require(back.base == obj.base, "single-chunk round trip lost the load address");
    require(back.textBytes == obj.textBytes, "single-chunk round trip lost the program bytes");
}

// A .text exactly 256 bytes long is the one case where the real xtrs cmd.c writer's own LENGTH
// byte wraps (256 + 2 = 258, truncated to a byte -> 2) - CmdFormat.h's own header comment on why
// a literal LENGTH of 0/1/2 can never otherwise occur on a type-1 record.
void testWraparoundChunkLength() {
    rcc::ObjectFile obj = makeObject(0x8000, 256);
    rcc::CmdFormatWriter writer;
    std::string bytes = writer.write(obj);

    require(static_cast<uint8_t>(bytes[0]) == 1, "expected a type-1 record first");
    require(static_cast<uint8_t>(bytes[1]) == 2, "256-byte chunk's LENGTH should wrap to 2 (258 & 0xFF)");

    rcc::CmdFormatReader reader;
    rcc::ObjectFile back = reader.read(bytes);
    require(back.base == obj.base, "256-byte chunk round trip lost the load address");
    require(back.textBytes == obj.textBytes, "256-byte chunk round trip lost the program bytes");
}

// A .text longer than 256 bytes is split across multiple contiguous type-1 records, each
// carrying its own load address picking up exactly where the previous one left off.
void testMultipleChunks() {
    rcc::ObjectFile obj = makeObject(0x9000, 300);
    rcc::CmdFormatWriter writer;
    std::string bytes = writer.write(obj);

    // First record: type 1, LENGTH 258 & 0xFF = 2, address $9000, 256 code bytes.
    require(static_cast<uint8_t>(bytes[0]) == 1, "first record should be type 1");
    require(static_cast<uint8_t>(bytes[1]) == 2, "first 256-byte chunk's LENGTH should wrap to 2");
    size_t secondRecordOffset = 4 + 256;
    require(static_cast<uint8_t>(bytes[secondRecordOffset]) == 1, "second record should be type 1");
    require(static_cast<uint8_t>(bytes[secondRecordOffset + 1]) == 46,  // 44 code bytes + 2
            "second (44-byte) chunk's LENGTH is wrong");
    uint32_t secondAddr = static_cast<uint8_t>(bytes[secondRecordOffset + 2]) |
                           (static_cast<uint8_t>(bytes[secondRecordOffset + 3]) << 8);
    require(secondAddr == obj.base + 256, "second chunk's load address doesn't continue the first");

    rcc::CmdFormatReader reader;
    rcc::ObjectFile back = reader.read(bytes);
    require(back.base == obj.base, "multi-chunk round trip lost the load address");
    require(back.textBytes == obj.textBytes, "multi-chunk round trip lost the program bytes");
}

void rejects(const std::string &bytes, const char *why) {
    rcc::CmdFormatReader reader;
    try {
        reader.read(bytes);
    } catch (const std::runtime_error &) {
        return;
    }
    throw std::runtime_error(std::string("should have rejected: ") + why);
}

void testMalformedInput() {
    // No type-2 transfer-address record at all.
    rejects(std::string(1, 1) + std::string(1, 3) + std::string("\x00\x60", 2) + "a", "missing transfer address");
    // An unsupported record type.
    rejects(std::string(1, 9) + std::string(1, 2) + std::string("\x00\x60", 2), "unsupported record type");
    // A second load block that doesn't continue the first one's address.
    std::string gap;
    gap.push_back(1);
    gap.push_back(3);
    gap.push_back(0x00);
    gap.push_back(0x60);
    gap.push_back('a');
    gap.push_back(1);
    gap.push_back(3);
    gap.push_back(0x05);
    gap.push_back(0x60);
    gap.push_back('b');
    gap.push_back(2);
    gap.push_back(2);
    gap.push_back(0x00);
    gap.push_back(0x60);
    rejects(gap, "non-contiguous load blocks");
}

}  // namespace

int main() {
    try {
        testSingleChunkRoundTrip();
        testWraparoundChunkLength();
        testMultipleChunks();
        testMalformedInput();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
