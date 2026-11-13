#include "fileformat/decb/DecbFormat.h"

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

// A program round-trips to exactly one data block (big-endian length/address, no wraparound,
// unlike CmdFormat's 256-byte TRS-80 quirk) followed by one EOF block whose transfer address is
// `base`.
void testRoundTrip() {
    rcc::ObjectFile obj = makeObject(0x3000, 300);
    rcc::DecbFormatWriter writer;
    std::string bytes = writer.write(obj);

    std::string expected;
    expected.push_back(0x00);
    expected.push_back(0x01);  // length 300 = 0x012C, big-endian
    expected.push_back(0x2C);
    expected.push_back(0x30);  // load address 0x3000, big-endian
    expected.push_back(0x00);
    expected.append(reinterpret_cast<const char *>(obj.textBytes.data()), obj.textBytes.size());
    expected.push_back(static_cast<char>(0xFF));
    expected.push_back(0x00);
    expected.push_back(0x00);  // length 0
    expected.push_back(0x30);
    expected.push_back(0x00);  // exec address 0x3000, big-endian
    require(bytes == expected, "round trip's byte layout doesn't match the expected data+EOF blocks");

    rcc::DecbFormatReader reader;
    rcc::ObjectFile back = reader.read(bytes);
    require(back.base == obj.base, "round trip lost the load address");
    require(back.textBytes == obj.textBytes, "round trip lost the program bytes");
}

// The reader accepts multiple contiguous data blocks even though the writer never emits more
// than one, for round-trip robustness against any future multi-segment producer.
void testMultipleDataBlocks() {
    std::string bytes;
    bytes.push_back(0x00);
    bytes.push_back(0x00);
    bytes.push_back(0x02);  // length 2
    bytes.push_back(0x40);
    bytes.push_back(0x00);  // address 0x4000
    bytes.push_back('a');
    bytes.push_back('b');
    bytes.push_back(0x00);
    bytes.push_back(0x00);
    bytes.push_back(0x01);  // length 1
    bytes.push_back(0x40);
    bytes.push_back(0x02);  // address 0x4002 - continues the first block
    bytes.push_back('c');
    bytes.push_back(static_cast<char>(0xFF));
    bytes.push_back(0x00);
    bytes.push_back(0x00);
    bytes.push_back(0x40);
    bytes.push_back(0x00);

    rcc::DecbFormatReader reader;
    rcc::ObjectFile obj = reader.read(bytes);
    require(obj.base == 0x4000, "multi-block read got the wrong load address");
    require(obj.textBytes.size() == 3 && obj.textBytes[0] == 'a' && obj.textBytes[1] == 'b' &&
                obj.textBytes[2] == 'c',
            "multi-block read didn't concatenate contiguous data blocks correctly");
}

void rejects(const std::string &bytes, const char *why) {
    rcc::DecbFormatReader reader;
    try {
        reader.read(bytes);
    } catch (const std::runtime_error &) {
        return;
    }
    throw std::runtime_error(std::string("should have rejected: ") + why);
}

void testMalformedInput() {
    // No EOF block at all.
    std::string noEof;
    noEof.push_back(0x00);
    noEof.push_back(0x00);
    noEof.push_back(0x01);
    noEof.push_back(0x30);
    noEof.push_back(0x00);
    noEof.push_back('a');
    rejects(noEof, "missing EOF block");

    // Truncated block header.
    rejects(std::string(3, '\x00'), "truncated block header");

    // A data block whose declared length runs past the end of the file.
    std::string truncatedData;
    truncatedData.push_back(0x00);
    truncatedData.push_back(0x00);
    truncatedData.push_back(0x10);  // claims 16 bytes
    truncatedData.push_back(0x30);
    truncatedData.push_back(0x00);
    truncatedData.push_back('a');  // only 1 byte actually present
    rejects(truncatedData, "data block running past end of file");

    // An unsupported block type.
    std::string badType;
    badType.push_back(0x7E);
    badType.push_back(0x00);
    badType.push_back(0x00);
    badType.push_back(0x30);
    badType.push_back(0x00);
    rejects(badType, "unsupported block type");

    // A second data block that doesn't continue the first one's address.
    std::string gap;
    gap.push_back(0x00);
    gap.push_back(0x00);
    gap.push_back(0x01);
    gap.push_back(0x30);
    gap.push_back(0x00);
    gap.push_back('a');
    gap.push_back(0x00);
    gap.push_back(0x00);
    gap.push_back(0x01);
    gap.push_back(0x30);
    gap.push_back(0x05);
    gap.push_back('b');
    gap.push_back(static_cast<char>(0xFF));
    gap.push_back(0x00);
    gap.push_back(0x00);
    gap.push_back(0x30);
    gap.push_back(0x00);
    rejects(gap, "non-contiguous data blocks");
}

}  // namespace

int main() {
    try {
        testRoundTrip();
        testMultipleDataBlocks();
        testMalformedInput();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
