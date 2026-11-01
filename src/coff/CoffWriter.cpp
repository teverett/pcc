#include "CoffWriter.h"

#include <stdexcept>
#include <unordered_map>

namespace pcc {

namespace {

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
void putBytes(std::string &out, const void *data, size_t n) {
    out.append(static_cast<const char *>(data), n);
}

// COFF's symbol Name field is a fixed 8 bytes: a short name goes
// there directly (NUL-padded); a longer one instead gets 4 zero
// bytes followed by a 4-byte offset into the string table.
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

}  // namespace

std::string writeCoffObject(uint16_t machineType, const std::vector<uint8_t> &textBytes,
                             const std::vector<CoffSymbol> &symbols,
                             const std::vector<CoffRelocation> &relocations) {
    // Build the string table (for symbol names > 8 chars) first, so
    // each long name's offset is known before writing the symbol
    // table itself. Offset 0 in the string table is reserved (means
    // "no name" by COFF convention), so real names start at offset 4
    // - which the leading 4-byte total-length field itself occupies.
    std::string stringTable;
    put32(stringTable, 0);  // placeholder; patched to the real total length below
    std::vector<uint32_t> nameOffsets(symbols.size(), 0);
    std::unordered_map<std::string, uint32_t> symbolIndex;  // name -> index, for relocations below
    for (size_t i = 0; i < symbols.size(); i++) {
        symbolIndex[symbols[i].name] = static_cast<uint32_t>(i);
        if (symbols[i].name.size() > 8) {
            nameOffsets[i] = static_cast<uint32_t>(stringTable.size());
            stringTable += symbols[i].name;
            stringTable.push_back('\0');
        }
    }
    // Patch the leading length field now that the real size is known.
    uint32_t stringTableLen = static_cast<uint32_t>(stringTable.size());
    stringTable[0] = static_cast<char>(stringTableLen & 0xFF);
    stringTable[1] = static_cast<char>((stringTableLen >> 8) & 0xFF);
    stringTable[2] = static_cast<char>((stringTableLen >> 16) & 0xFF);
    stringTable[3] = static_cast<char>((stringTableLen >> 24) & 0xFF);

    constexpr uint32_t kFileHeaderSize = 20;
    constexpr uint32_t kSectionHeaderSize = 40;
    constexpr uint32_t kRelocationSize = 10;  // VirtualAddress(4) + SymbolTableIndex(4) + Type(2)

    uint32_t textOffset = kFileHeaderSize + kSectionHeaderSize;
    uint32_t relocOffset = textOffset + static_cast<uint32_t>(textBytes.size());
    uint32_t numRelocs = static_cast<uint32_t>(relocations.size());
    uint32_t symTableOffset = relocOffset + numRelocs * kRelocationSize;
    uint32_t numSymbols = static_cast<uint32_t>(symbols.size());

    std::string out;

    // --- IMAGE_FILE_HEADER (20 bytes) ---
    put16(out, machineType);
    put16(out, 1);                  // NumberOfSections - just .text
    put32(out, 0);                   // TimeDateStamp
    put32(out, symTableOffset);       // PointerToSymbolTable
    put32(out, numSymbols);            // NumberOfSymbols
    put16(out, 0);                      // SizeOfOptionalHeader - none (object file)
    put16(out, 0);                       // Characteristics

    // --- IMAGE_SECTION_HEADER (40 bytes) for ".text" ---
    char name[8] = {'.', 't', 'e', 'x', 't', 0, 0, 0};
    putBytes(out, name, 8);
    put32(out, 0);                                        // PhysicalAddress/VirtualSize
    put32(out, 0);                                          // VirtualAddress
    put32(out, static_cast<uint32_t>(textBytes.size()));      // SizeOfRawData
    put32(out, textOffset);                                    // PointerToRawData
    put32(out, numRelocs ? relocOffset : 0);                     // PointerToRelocations
    put32(out, 0);                                                // PointerToLinenumbers
    put16(out, static_cast<uint16_t>(numRelocs));                  // NumberOfRelocations
    put16(out, 0);                                                  // NumberOfLinenumbers
    put32(out, 0x60000020);  // IMAGE_SCN_CNT_CODE | MEM_EXECUTE | MEM_READ

    // --- .text raw bytes ---
    if (!textBytes.empty()) putBytes(out, textBytes.data(), textBytes.size());

    // --- relocation table (10 bytes/entry, IMAGE_RELOCATION shape) ---
    for (const auto &r : relocations) {
        auto it = symbolIndex.find(r.symbolName);
        if (it == symbolIndex.end()) {
            throw std::runtime_error("CoffWriter: relocation references unknown symbol '" + r.symbolName + "'");
        }
        put32(out, r.offset);
        put32(out, it->second);
        put16(out, r.type);
    }

    // --- symbol table (18 bytes/entry) ---
    for (size_t i = 0; i < symbols.size(); i++) {
        putSymbolName(out, symbols[i].name, nameOffsets[i]);
        put32(out, symbols[i].value);                     // Value
        put16(out, symbols[i].isDefined ? 1 : 0);            // SectionNumber - .text, or 0 (undefined)
        put16(out, 0);                                        // Type
        out.push_back(static_cast<char>(2));  // StorageClass = C_EXT (external/global)
        out.push_back(static_cast<char>(0));   // NumberOfAuxSymbols
    }

    // --- string table ---
    out += stringTable;

    return out;
}

}  // namespace pcc
