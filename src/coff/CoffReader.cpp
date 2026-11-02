#include "CoffReader.h"

#include <stdexcept>

namespace pcc {

namespace {

[[noreturn]] void fail(const std::string &msg) { throw std::runtime_error("CoffReader: " + msg); }

void need(const std::string &bytes, size_t offset, size_t len) {
    if (offset + len > bytes.size()) fail("truncated object file");
}

uint16_t get16(const std::string &bytes, size_t offset) {
    need(bytes, offset, 2);
    return static_cast<uint16_t>(static_cast<uint8_t>(bytes[offset]) |
                                  (static_cast<uint8_t>(bytes[offset + 1]) << 8));
}
uint32_t get32(const std::string &bytes, size_t offset) {
    need(bytes, offset, 4);
    return static_cast<uint32_t>(static_cast<uint8_t>(bytes[offset])) |
           (static_cast<uint32_t>(static_cast<uint8_t>(bytes[offset + 1])) << 8) |
           (static_cast<uint32_t>(static_cast<uint8_t>(bytes[offset + 2])) << 16) |
           (static_cast<uint32_t>(static_cast<uint8_t>(bytes[offset + 3])) << 24);
}

// Mirrors CoffWriter.cpp's putSymbolName: a short name sits directly
// in the 8-byte field (NUL-padded); a long one is flagged by a
// leading 4 zero bytes, with the real name read from the string table
// at the offset the following 4 bytes give.
std::string getSymbolName(const std::string &bytes, size_t nameFieldOffset, const std::string &stringTable) {
    need(bytes, nameFieldOffset, 8);
    uint32_t firstWord = get32(bytes, nameFieldOffset);
    if (firstWord != 0) {
        size_t end = nameFieldOffset;
        while (end < nameFieldOffset + 8 && bytes[end] != '\0') end++;
        return bytes.substr(nameFieldOffset, end - nameFieldOffset);
    }
    uint32_t stringOffset = get32(bytes, nameFieldOffset + 4);
    if (stringOffset >= stringTable.size()) fail("symbol name offset outside string table");
    size_t end = stringTable.find('\0', stringOffset);
    if (end == std::string::npos) fail("unterminated string-table entry");
    return stringTable.substr(stringOffset, end - stringOffset);
}

}  // namespace

ObjectFile readCoffObject(const std::string &bytes) {
    constexpr size_t kFileHeaderSize = 20;
    constexpr size_t kSectionHeaderSize = 40;
    constexpr size_t kRelocationSize = 10;
    constexpr size_t kSymbolSize = 18;

    need(bytes, 0, kFileHeaderSize);
    ObjectFile obj;
    obj.machineType = get16(bytes, 0);
    uint16_t numSections = get16(bytes, 2);
    if (numSections != 2) {
        fail("expected exactly two sections (.text, .bss), got " + std::to_string(numSections));
    }
    uint32_t symTableOffset = get32(bytes, 8);
    uint32_t numSymbols = get32(bytes, 12);

    // ".text" (section 1 in the symbol table's own SectionNumber
    // field - see CoffWriter.cpp's own layout) and ".bss" (section 2,
    // a pure Harvard-data-space size reservation, never any raw bytes
    // of its own - see that file's own comment) are always written in
    // that fixed order, never found by name/type the way
    // ElfObjectFormat.cpp's own reader more flexibly does.
    size_t sectionHeaderOffset = kFileHeaderSize;
    need(bytes, sectionHeaderOffset, kSectionHeaderSize);
    uint32_t sizeOfRawData = get32(bytes, sectionHeaderOffset + 16);
    uint32_t pointerToRawData = get32(bytes, sectionHeaderOffset + 20);
    uint32_t pointerToRelocations = get32(bytes, sectionHeaderOffset + 24);
    uint16_t numRelocations = get16(bytes, sectionHeaderOffset + 32);

    size_t bssHeaderOffset = sectionHeaderOffset + kSectionHeaderSize;
    need(bytes, bssHeaderOffset, kSectionHeaderSize);
    obj.dataSize = get32(bytes, bssHeaderOffset + 8);  // PhysicalAddress/VirtualSize

    need(bytes, pointerToRawData, sizeOfRawData);
    obj.textBytes.assign(bytes.begin() + static_cast<long>(pointerToRawData),
                          bytes.begin() + static_cast<long>(pointerToRawData + sizeOfRawData));

    // String table: immediately after the symbol table, a 4-byte
    // total-length prefix followed by that many more bytes (see
    // CoffWriter.cpp's own layout) - read it first since symbol names
    // may point into it.
    size_t stringTableOffset = symTableOffset + static_cast<size_t>(numSymbols) * kSymbolSize;
    need(bytes, stringTableOffset, 4);
    uint32_t stringTableLen = get32(bytes, stringTableOffset);
    need(bytes, stringTableOffset, stringTableLen);
    std::string stringTable = bytes.substr(stringTableOffset, stringTableLen);

    obj.symbols.reserve(numSymbols);
    for (uint32_t i = 0; i < numSymbols; i++) {
        size_t symOffset = symTableOffset + static_cast<size_t>(i) * kSymbolSize;
        need(bytes, symOffset, kSymbolSize);
        std::string name = getSymbolName(bytes, symOffset, stringTable);
        uint32_t value = get32(bytes, symOffset + 8);
        uint16_t sectionNumber = get16(bytes, symOffset + 12);
        obj.symbols.push_back({name, value, /*isDefined=*/sectionNumber != 0, /*isData=*/sectionNumber == 2});
    }

    // Relocations reference symbols by table index (SymbolTableIndex)
    // - resolved back to a name immediately, since the linker only
    // ever wants to look relocations up by name (see CoffReader.h).
    obj.relocations.reserve(numRelocations);
    for (uint16_t i = 0; i < numRelocations; i++) {
        size_t relOffset = pointerToRelocations + static_cast<size_t>(i) * kRelocationSize;
        need(bytes, relOffset, kRelocationSize);
        uint32_t fieldOffset = get32(bytes, relOffset);
        uint32_t symbolIndex = get32(bytes, relOffset + 4);
        uint16_t type = get16(bytes, relOffset + 8);
        if (symbolIndex >= obj.symbols.size()) fail("relocation references symbol index out of range");
        obj.relocations.push_back({fieldOffset, obj.symbols[symbolIndex].name, type});
    }

    return obj;
}

}  // namespace pcc
