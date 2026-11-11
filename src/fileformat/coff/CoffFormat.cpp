#include "CoffFormat.h"

#include <stdexcept>
#include <unordered_map>

namespace rcc {

namespace {

[[noreturn]] void fail(const std::string &msg) { throw std::runtime_error("CoffFormat: " + msg); }

constexpr uint16_t kI386WindowsMachine = 0x014C;
constexpr uint16_t kRccI386Machine = 0x3386;
constexpr uint16_t kI386RelocAbs32 = 1;
constexpr uint16_t kI386RelocRel32 = 2;
constexpr uint16_t kImageRelI386Dir32 = 0x0006;
constexpr uint16_t kImageRelI386Rel32 = 0x0014;

bool isRccLocalSymbol(const std::string &name) {
    return name.compare(0, 12, "__rcc_local_") == 0 || name.compare(0, 10, "__rcc_tmp_") == 0;
}

void put16(std::string &out, uint16_t v) {
    out.push_back(static_cast<char>(v & 0xFF));
    out.push_back(static_cast<char>((v >> 8) & 0xFF));
}
void put32(std::string &out, uint32_t v) {
    out.push_back(static_cast<char>(v & 0xFF));
    out.push_back(static_cast<char>((v >> 8) & 0xFF));
    out.push_back(static_cast<char>((v >> 16) & 0xFF));
    out.push_back(static_cast<char>((v >> 24) & 0xFF));
}
void putBytes(std::string &out, const void *data, size_t n) { out.append(static_cast<const char *>(data), n); }

// Symbol Name is a fixed 8 bytes: <=8 chars go there NUL-padded; longer names get 4 zero bytes
// then a 4-byte string-table offset.
void putSymbolName(std::string &out, const std::string &name, uint32_t stringTableOffset) {
    if (name.size() <= 8) {
        char buf[8] = {0};
        for (size_t i = 0; i < name.size(); i++) buf[i] = name[i];
        putBytes(out, buf, 8);
    } else {
        put32(out, 0);
        put32(out, stringTableOffset);
    }
}

void need(const std::string &bytes, size_t offset, size_t len) {
    if (offset + len > bytes.size()) fail("truncated file");
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

// Inverse of putSymbolName.
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

std::string CoffFormatWriter::write(const ObjectFile &obj) const {
    const bool windowsI386 = obj.machineType == kI386WindowsMachine;
    // Built first so each long name's string-table offset is known before the symbol table is
    // written. Offset 0 is reserved ("no name"); the leading 4-byte length field occupies it.
    std::string stringTable;
    put32(stringTable, 0);  // placeholder; patched below
    std::vector<uint32_t> nameOffsets(obj.symbols.size(), 0);
    std::unordered_map<std::string, uint32_t> symbolIndex;  // name -> index, for relocations below
    uint32_t numSymbols = 0;
    for (size_t i = 0; i < obj.symbols.size(); i++) {
        const std::string &name = obj.symbols[i].name;
        symbolIndex[name] = numSymbols;
        if (windowsI386 && name == "..") symbolIndex[".text"] = numSymbols;
        const std::string &serializedName = windowsI386 && name == ".." ? ".text" : name;
        if (serializedName.size() > 8) {
            nameOffsets[i] = static_cast<uint32_t>(stringTable.size());
            stringTable += serializedName;
            stringTable.push_back('\0');
        }
        numSymbols += 1 + (windowsI386 && name == ".." ? 1 : 0);
    }
    uint32_t stringTableLen = static_cast<uint32_t>(stringTable.size());
    stringTable[0] = static_cast<char>(stringTableLen & 0xFF);
    stringTable[1] = static_cast<char>((stringTableLen >> 8) & 0xFF);
    stringTable[2] = static_cast<char>((stringTableLen >> 16) & 0xFF);
    stringTable[3] = static_cast<char>((stringTableLen >> 24) & 0xFF);

    constexpr uint32_t kFileHeaderSize = 20;
    constexpr uint32_t kSectionHeaderSize = 40;
    constexpr uint32_t kRelocationSize = 10;  // VirtualAddress(4) + SymbolTableIndex(4) + Type(2)
    constexpr uint32_t kNumSections = 2;      // ".text", ".bss"

    uint32_t textOffset = kFileHeaderSize + kNumSections * kSectionHeaderSize;
    uint32_t relocOffset = textOffset + static_cast<uint32_t>(obj.textBytes.size());
    uint32_t numRelocs = static_cast<uint32_t>(obj.relocations.size());
    uint32_t symTableOffset = relocOffset + numRelocs * kRelocationSize;

    std::string out;

    // IMAGE_FILE_HEADER
    put16(out, obj.machineType);
    put16(out, static_cast<uint16_t>(kNumSections));
    put32(out, 0);                   // TimeDateStamp
    put32(out, symTableOffset);       // PointerToSymbolTable
    put32(out, numSymbols);            // NumberOfSymbols
    put16(out, 0);                      // SizeOfOptionalHeader - none (object file)
    put16(out, 0);                       // Characteristics

    // IMAGE_SECTION_HEADER for ".text"
    char name[8] = {'.', 't', 'e', 'x', 't', 0, 0, 0};
    putBytes(out, name, 8);
    put32(out, 0);                                            // PhysicalAddress/VirtualSize
    put32(out, 0);                                              // VirtualAddress
    put32(out, static_cast<uint32_t>(obj.textBytes.size()));      // SizeOfRawData
    put32(out, textOffset);                                        // PointerToRawData
    put32(out, numRelocs ? relocOffset : 0);                         // PointerToRelocations
    put32(out, 0);                                                    // PointerToLinenumbers
    put16(out, static_cast<uint16_t>(numRelocs));                      // NumberOfRelocations
    put16(out, 0);                                                      // NumberOfLinenumbers
    put32(out, 0x60000020);  // IMAGE_SCN_CNT_CODE | MEM_EXECUTE | MEM_READ

    // IMAGE_SECTION_HEADER for ".bss": pure size reservation, no raw file bytes
    // (PointerToRawData/SizeOfRawData=0), same as IMAGE_SCN_CNT_UNINITIALIZED_DATA.
    char bssName[8] = {'.', 'b', 's', 's', 0, 0, 0, 0};
    putBytes(out, bssName, 8);
    put32(out, obj.dataSize);  // PhysicalAddress/VirtualSize
    put32(out, 0);              // VirtualAddress
    put32(out, 0);               // SizeOfRawData
    put32(out, 0);                // PointerToRawData
    put32(out, 0);                 // PointerToRelocations
    put32(out, 0);                  // PointerToLinenumbers
    put16(out, 0);                   // NumberOfRelocations
    put16(out, 0);                    // NumberOfLinenumbers
    put32(out, 0xC0000080);  // IMAGE_SCN_CNT_UNINITIALIZED_DATA | MEM_READ | MEM_WRITE

    if (!obj.textBytes.empty()) putBytes(out, obj.textBytes.data(), obj.textBytes.size());

    for (const auto &r : obj.relocations) {
        auto it = symbolIndex.find(r.symbolName);
        if (it == symbolIndex.end()) fail("relocation references unknown symbol '" + r.symbolName + "'");
        put32(out, r.offset);
        put32(out, it->second);
        uint16_t type = r.type;
        if (windowsI386) {
            if (r.type == kI386RelocAbs32) {
                type = kImageRelI386Dir32;
            } else if (r.type == kI386RelocRel32) {
                type = kImageRelI386Rel32;
            } else {
                fail("unsupported hosted Windows i386 relocation " + std::to_string(r.type));
            }
        }
        put16(out, type);
    }

    for (size_t i = 0; i < obj.symbols.size(); i++) {
        const ObjSymbol &sym = obj.symbols[i];
        const bool sectionSymbol = windowsI386 && sym.name == "..";
        std::string serializedName = sectionSymbol ? ".text" : sym.name;
        putSymbolName(out, serializedName, nameOffsets[i]);
        put32(out, sectionSymbol ? 0 : sym.value);  // Value
        // SectionNumber: 0 (undefined), 1 (.text), or 2 (.bss).
        put16(out, sectionSymbol ? 1 : (!sym.isDefined ? 0 : (sym.isData ? 2 : 1)));
        // Type: unused by any real COFF consumer here, so bit 0 carries ObjSymbol::isRoot across
        // the round trip through disk (GenericLinker.cpp's --gc-sections needs it on the read-back
        // side, same as every other ObjSymbol field).
        put16(out, sym.isRoot ? 1 : 0);
        const bool local = windowsI386 && (sectionSymbol || isRccLocalSymbol(sym.name));
        out.push_back(static_cast<char>(local ? 3 : 2));  // C_STAT or C_EXT
        out.push_back(static_cast<char>(sectionSymbol ? 1 : 0));  // NumberOfAuxSymbols
        if (sectionSymbol) {
            put32(out, static_cast<uint32_t>(obj.textBytes.size()));
            put16(out, static_cast<uint16_t>(numRelocs));
            put16(out, 0);  // NumberOfLinenumbers
            put32(out, 0);  // Checksum
            put16(out, 1);  // Number
            out.push_back(0);  // Selection
            putBytes(out, std::string(3, '\0').data(), 3);
        }
    }

    out += stringTable;

    return out;
}

ObjectFile CoffFormatReader::read(const std::string &bytes) const {
    constexpr size_t kFileHeaderSize = 20;
    constexpr size_t kSectionHeaderSize = 40;
    constexpr size_t kRelocationSize = 10;
    constexpr size_t kSymbolSize = 18;

    need(bytes, 0, kFileHeaderSize);
    ObjectFile obj;
    const bool windowsI386 = get16(bytes, 0) == kI386WindowsMachine;
    obj.machineType = windowsI386 ? kRccI386Machine : get16(bytes, 0);
    uint16_t numSections = get16(bytes, 2);
    if (numSections != 2) {
        fail("expected exactly two sections (.text, .bss), got " + std::to_string(numSections));
    }
    uint32_t symTableOffset = get32(bytes, 8);
    uint32_t numSymbols = get32(bytes, 12);

    // ".text" and ".bss" are always written in this fixed order (unlike ElfFormat.cpp's
    // reader, which finds sections by type).
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

    // String table immediately follows the symbol table: a 4-byte length prefix then that many bytes.
    size_t stringTableOffset = symTableOffset + static_cast<size_t>(numSymbols) * kSymbolSize;
    need(bytes, stringTableOffset, 4);
    uint32_t stringTableLen = get32(bytes, stringTableOffset);
    need(bytes, stringTableOffset, stringTableLen);
    std::string stringTable = bytes.substr(stringTableOffset, stringTableLen);

    obj.symbols.reserve(numSymbols);
    std::vector<uint32_t> symbolIndices(numSymbols, UINT32_MAX);
    for (uint32_t i = 0; i < numSymbols;) {
        size_t symOffset = symTableOffset + static_cast<size_t>(i) * kSymbolSize;
        need(bytes, symOffset, kSymbolSize);
        std::string name = getSymbolName(bytes, symOffset, stringTable);
        uint32_t value = get32(bytes, symOffset + 8);
        uint16_t sectionNumber = get16(bytes, symOffset + 12);
        uint16_t type = get16(bytes, symOffset + 14);
        uint8_t storageClass = static_cast<uint8_t>(bytes[symOffset + 16]);
        uint8_t auxiliaryCount = static_cast<uint8_t>(bytes[symOffset + 17]);
        if (auxiliaryCount > numSymbols - i - 1) fail("symbol auxiliary count exceeds symbol table");
        if (windowsI386 && name == ".text" && sectionNumber == 1 && storageClass == 3) {
            name = "..";
            value = 0;
        }
        symbolIndices[i] = static_cast<uint32_t>(obj.symbols.size());
        obj.symbols.push_back({name, value, /*isDefined=*/sectionNumber != 0, /*isData=*/sectionNumber == 2,
                                /*isBss=*/false, /*isRoot=*/(type & 1) != 0});
        i += static_cast<uint32_t>(auxiliaryCount) + 1;
    }

    obj.relocations.reserve(numRelocations);
    for (uint16_t i = 0; i < numRelocations; i++) {
        size_t relOffset = pointerToRelocations + static_cast<size_t>(i) * kRelocationSize;
        need(bytes, relOffset, kRelocationSize);
        uint32_t fieldOffset = get32(bytes, relOffset);
        uint32_t symbolIndex = get32(bytes, relOffset + 4);
        uint16_t type = get16(bytes, relOffset + 8);
        if (symbolIndex >= symbolIndices.size() || symbolIndices[symbolIndex] == UINT32_MAX) {
            fail("relocation references symbol index out of range");
        }
        if (windowsI386) {
            if (type == kImageRelI386Dir32) type = kI386RelocAbs32;
            else if (type == kImageRelI386Rel32) type = kI386RelocRel32;
        }
        obj.relocations.push_back({fieldOffset, obj.symbols[symbolIndices[symbolIndex]].name, type});
    }

    return obj;
}

}  // namespace rcc
