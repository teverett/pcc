#include "MachOFormat.h"

#include <cstdint>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#include "codegen/arm64/Arm64Assembler.h"

namespace rcc {

namespace {

[[noreturn]] void fail(const std::string &msg) { throw std::runtime_error("MachOFormat: " + msg); }

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
void put64(std::string &out, uint64_t v) {
    put32(out, static_cast<uint32_t>(v));
    put32(out, static_cast<uint32_t>(v >> 32));
}
void putName16(std::string &out, const char *name) {
    char buf[16] = {0};
    for (size_t i = 0; name[i] != '\0' && i < 16; i++) buf[i] = name[i];
    out.append(buf, 16);
}
void putBytes(std::string &out, const void *data, size_t n) { out.append(static_cast<const char *>(data), n); }

constexpr uint32_t kMhMagic = 0xFEEDFACE;
constexpr uint32_t kMhObject = 0x1;
constexpr uint32_t kLcSegment = 0x1;
constexpr uint32_t kLcSymtab = 0x2;

constexpr size_t kMachHeaderSize = 28;
constexpr size_t kSegmentCmdSize = 56;
constexpr size_t kSectionSize = 68;
constexpr size_t kSymtabCmdSize = 24;
constexpr size_t kNlistSize = 12;
constexpr size_t kRelocSize = 8;

bool isArm64DarwinLocalSymbol(const std::string &name) {
    return name.rfind("L", 0) == 0 || name.rfind("___strlit", 0) == 0;
}

std::string writeArm64DarwinObject(const ObjectFile &obj) {
    constexpr uint32_t kMhMagic64 = 0xFEEDFACF;
    constexpr uint32_t kCpuTypeArm64 = 0x0100000C;
    constexpr uint32_t kLcSegment64 = 0x19;
    constexpr uint32_t kLcBuildVersion = 0x32;
    constexpr uint32_t kSegment64Size = 72;
    constexpr uint32_t kSection64Size = 80;
    constexpr uint32_t kMachHeader64Size = 32;
    constexpr uint32_t kRelocBranch26 = 2;
    constexpr uint32_t kRelocPage21 = 3;
    constexpr uint32_t kRelocPageoff12 = 4;

    std::unordered_map<std::string, uint32_t> symIndex;
    for (size_t i = 0; i < obj.symbols.size(); i++) {
        symIndex[obj.symbols[i].name] = static_cast<uint32_t>(i);
    }

    std::string strtab(1, '\0');
    std::vector<uint32_t> nameOffsets(obj.symbols.size());
    for (size_t i = 0; i < obj.symbols.size(); i++) {
        nameOffsets[i] = static_cast<uint32_t>(strtab.size());
        strtab += obj.symbols[i].name;
        strtab.push_back('\0');
    }

    std::string text = obj.textBytes.empty()
                           ? std::string()
                           : std::string(reinterpret_cast<const char *>(obj.textBytes.data()), obj.textBytes.size());
    std::string relocations;
    for (const auto &reloc : obj.relocations) {
        auto it = symIndex.find(reloc.symbolName);
        if (it == symIndex.end()) fail("relocation references unknown symbol '" + reloc.symbolName + "'");
        uint32_t type;
        bool pcrel;
        if (reloc.type == kArm64DarwinRelocBranch26) {
            type = kRelocBranch26;
            pcrel = true;
        } else if (reloc.type == kArm64DarwinRelocPage21) {
            type = kRelocPage21;
            pcrel = true;
        } else if (reloc.type == kArm64DarwinRelocPageoff12) {
            type = kRelocPageoff12;
            pcrel = false;
        } else {
            fail("unsupported arm64 macOS relocation " + std::to_string(reloc.type));
        }

        put32(relocations, reloc.offset);
        uint32_t word = (it->second & 0xFFFFFFu) | (static_cast<uint32_t>(pcrel) << 24) | (2u << 25) |
                        (1u << 27) | ((type & 0xFu) << 28);
        put32(relocations, word);
    }

    uint64_t dataAddress = (text.size() + 7u) & ~7ULL;
    uint64_t bssAddress = dataAddress + obj.dataBytes.size();
    std::string symtab;
    for (size_t i = 0; i < obj.symbols.size(); i++) {
        const ObjSymbol &symbol = obj.symbols[i];
        bool local = symbol.isDefined && isArm64DarwinLocalSymbol(symbol.name);
        put32(symtab, nameOffsets[i]);
        symtab.push_back(static_cast<char>(symbol.isDefined ? (local ? 0x0E : 0x0F) : 0x01));
        uint8_t section = symbol.isBss ? static_cast<uint8_t>(2 + !obj.dataBytes.empty()) :
                          symbol.isData ? 2 : 1;
        symtab.push_back(static_cast<char>(symbol.isDefined ? section : 0));
        put16(symtab, 0);
        uint64_t value = symbol.value;
        if (symbol.isDefined && symbol.isData) {
            value += symbol.isBss ? bssAddress : dataAddress;
        }
        put64(symtab, symbol.isDefined ? value : 0);
    }

    const uint32_t textSegmentSize = kSegment64Size + kSection64Size;
    const uint32_t dataSectionCount = static_cast<uint32_t>(!obj.dataBytes.empty()) +
                                      static_cast<uint32_t>(obj.dataSize != 0);
    const uint32_t dataSegmentSize = dataSectionCount == 0 ? 0 :
                                     kSegment64Size + dataSectionCount * kSection64Size;
    constexpr uint32_t buildVersionSize = 24;
    uint32_t commandBytes = textSegmentSize + dataSegmentSize + buildVersionSize + kSymtabCmdSize;
    uint32_t textOff = kMachHeader64Size + commandBytes;
    uint32_t dataOff = (textOff + static_cast<uint32_t>(text.size()) + 7u) & ~7u;
    uint32_t relOff = dataOff + static_cast<uint32_t>(obj.dataBytes.size());
    uint32_t symOff = relOff + static_cast<uint32_t>(relocations.size());
    uint32_t strOff = symOff + static_cast<uint32_t>(symtab.size());
    std::string out;

    put32(out, kMhMagic64);
    put32(out, kCpuTypeArm64);
    put32(out, 0);
    put32(out, kMhObject);
    put32(out, 3 + static_cast<uint32_t>(dataSegmentSize != 0));
    put32(out, commandBytes);
    put32(out, 0);
    put32(out, 0);

    put32(out, kLcSegment64);
    put32(out, textSegmentSize);
    putName16(out, "__TEXT");
    put64(out, 0);
    put64(out, text.size());
    put64(out, textOff);
    put64(out, text.size());
    put32(out, 7);
    put32(out, 7);
    put32(out, 1);
    put32(out, 0);

    putName16(out, "__text");
    putName16(out, "__TEXT");
    put64(out, 0);
    put64(out, text.size());
    put32(out, textOff);
    put32(out, 2);
    put32(out, relocations.empty() ? 0 : relOff);
    put32(out, static_cast<uint32_t>(obj.relocations.size()));
    put32(out, 0);
    put32(out, 0);
    put32(out, 0);
    put32(out, 0);

    if (dataSegmentSize != 0) {
        put32(out, kLcSegment64);
        put32(out, dataSegmentSize);
        putName16(out, "__DATA");
        put64(out, dataAddress);
        put64(out, obj.dataBytes.size() + obj.dataSize);
        put64(out, dataOff);
        put64(out, obj.dataBytes.size());
        put32(out, 3);
        put32(out, 3);
        put32(out, dataSectionCount);
        put32(out, 0);
        if (!obj.dataBytes.empty()) {
            putName16(out, "__data");
            putName16(out, "__DATA");
            put64(out, dataAddress);
            put64(out, obj.dataBytes.size());
            put32(out, dataOff);
            put32(out, 3);
            put32(out, 0);
            put32(out, 0);
            put32(out, 0);
            put32(out, 0);
            put32(out, 0);
            put32(out, 0);
        }
        if (obj.dataSize != 0) {
            putName16(out, "__bss");
            putName16(out, "__DATA");
            put64(out, bssAddress);
            put64(out, obj.dataSize);
            put32(out, 0);
            put32(out, 3);
            put32(out, 0);
            put32(out, 0);
            put32(out, 1);
            put32(out, 0);
            put32(out, 0);
            put32(out, 0);
        }
    }

    put32(out, kLcBuildVersion);
    put32(out, buildVersionSize);
    put32(out, 1);
    put32(out, 0x000B0000);
    put32(out, 0x000F0000);
    put32(out, 0);

    put32(out, kLcSymtab);
    put32(out, static_cast<uint32_t>(kSymtabCmdSize));
    put32(out, symOff);
    put32(out, static_cast<uint32_t>(obj.symbols.size()));
    put32(out, strOff);
    put32(out, static_cast<uint32_t>(strtab.size()));

    out += text;
    out.append(dataOff - (textOff + text.size()), '\0');
    if (!obj.dataBytes.empty()) {
        out.append(reinterpret_cast<const char *>(obj.dataBytes.data()), obj.dataBytes.size());
    }
    out += relocations;
    out += symtab;
    out += strtab;
    return out;
}

}  // namespace

std::string MachOFormatWriter::write(const ObjectFile &obj) const {
    if (obj.machineType == kArm64DarwinMachine) return writeArm64DarwinObject(obj);
    for (const auto &r : obj.relocations) {
        if (r.type > 0xF) fail("relocation type " + std::to_string(r.type) + " doesn't fit in Mach-O's 4-bit r_type");
    }

    std::string strtab;
    strtab.push_back('\0');  // offset 0 conventionally means "no name"
    std::vector<uint32_t> nameOffsets(obj.symbols.size());
    for (size_t i = 0; i < obj.symbols.size(); i++) {
        nameOffsets[i] = static_cast<uint32_t>(strtab.size());
        strtab += obj.symbols[i].name;
        strtab.push_back('\0');
    }
    // Unlike COFF/ELF, Mach-O has no mandatory null entry: ObjSymbol i is nlist index i directly.
    std::unordered_map<std::string, uint32_t> symIndex;
    for (size_t i = 0; i < obj.symbols.size(); i++) symIndex[obj.symbols[i].name] = static_cast<uint32_t>(i);

    std::string reloctab;
    for (const auto &r : obj.relocations) {
        auto it = symIndex.find(r.symbolName);
        if (it == symIndex.end()) fail("relocation references unknown symbol '" + r.symbolName + "'");
        put32(reloctab, r.offset);  // r_address
        // Bitfield word: r_symbolnum(24) | r_pcrel(1)<<24 | r_length(2)<<25 | r_extern(1)<<27 | r_type(4)<<28.
        // r_length=1 means 2 bytes (every relocation patches a 16-bit field).
        uint32_t word = (it->second & 0xFFFFFFu) | (0u << 24) | (1u << 25) | (1u << 27) |
                         ((static_cast<uint32_t>(r.type) & 0xF) << 28);
        put32(reloctab, word);
    }

    std::string symtab;
    for (size_t i = 0; i < obj.symbols.size(); i++) {
        const ObjSymbol &sym = obj.symbols[i];
        put32(symtab, nameOffsets[i]);  // n_strx
        // N_EXT always set. Defined: N_SECT, n_sect=1(__text)/2(__bss). Undefined: N_UNDF, n_sect=NO_SECT.
        symtab.push_back(static_cast<char>(sym.isDefined ? (0x0e | 0x01) : (0x00 | 0x01)));  // n_type
        symtab.push_back(static_cast<char>(!sym.isDefined ? 0 : (sym.isData ? 2 : 1)));        // n_sect
        put16(symtab, 0);                                                                      // n_desc
        put32(symtab, sym.isDefined ? sym.value : 0);                                         // n_value
    }

    uint32_t textOff =
        static_cast<uint32_t>(kMachHeaderSize + kSegmentCmdSize + 2 * kSectionSize + kSymtabCmdSize);
    uint32_t relOff = textOff + static_cast<uint32_t>(obj.textBytes.size());
    uint32_t symOff = relOff + static_cast<uint32_t>(reloctab.size());
    uint32_t strOff = symOff + static_cast<uint32_t>(symtab.size());

    std::string out;

    put32(out, kMhMagic);
    put32(out, obj.machineType);  // cputype
    put32(out, 0);                 // cpusubtype
    put32(out, kMhObject);          // filetype
    put32(out, 2);                   // ncmds
    put32(out, static_cast<uint32_t>(kSegmentCmdSize + 2 * kSectionSize + kSymtabCmdSize));  // sizeofcmds
    put32(out, 0);                    // flags

    put32(out, kLcSegment);
    put32(out, static_cast<uint32_t>(kSegmentCmdSize + 2 * kSectionSize));
    putName16(out, "");  // segname
    put32(out, 0);        // vmaddr
    put32(out, static_cast<uint32_t>(obj.textBytes.size()) + obj.dataSize);  // vmsize
    put32(out, textOff);                                       // fileoff
    put32(out, static_cast<uint32_t>(obj.textBytes.size()));    // filesize (__bss has none)
    put32(out, 7);                                                // maxprot (rwx)
    put32(out, 7);                                                 // initprot
    put32(out, 2);                                                  // nsects
    put32(out, 0);                                                   // flags

    putName16(out, "__text");
    putName16(out, "");  // segname
    put32(out, 0);         // addr
    put32(out, static_cast<uint32_t>(obj.textBytes.size()));  // size
    put32(out, textOff);                                        // offset
    put32(out, 0);                                               // align (2^0 = 1 byte)
    put32(out, obj.relocations.empty() ? 0 : relOff);             // reloff
    put32(out, static_cast<uint32_t>(obj.relocations.size()));     // nreloc
    put32(out, 0);                                                   // flags (S_REGULAR)
    put32(out, 0);                                                    // reserved1
    put32(out, 0);                                                     // reserved2

    // S_ZEROFILL (flags=1): has a size but no file bytes, so offset is left 0.
    putName16(out, "__bss");
    putName16(out, "");
    put32(out, static_cast<uint32_t>(obj.textBytes.size()));  // addr: right after __text
    put32(out, obj.dataSize);                                   // size
    put32(out, 0);                                               // offset
    put32(out, 0);                                                // align
    put32(out, 0);                                                 // reloff
    put32(out, 0);                                                  // nreloc
    put32(out, 1);                                                   // flags (S_ZEROFILL)
    put32(out, 0);                                                     // reserved1
    put32(out, 0);                                                      // reserved2

    put32(out, kLcSymtab);
    put32(out, static_cast<uint32_t>(kSymtabCmdSize));
    put32(out, symOff);
    put32(out, static_cast<uint32_t>(obj.symbols.size()));
    put32(out, strOff);
    put32(out, static_cast<uint32_t>(strtab.size()));

    if (!obj.textBytes.empty()) putBytes(out, obj.textBytes.data(), obj.textBytes.size());
    out += reloctab;
    out += symtab;
    out += strtab;

    return out;
}

namespace {
void need(const std::string &bytes, size_t offset, size_t len) {
    if (offset + len > bytes.size()) fail("truncated object file");
}
uint32_t get32(const std::string &bytes, size_t offset) {
    need(bytes, offset, 4);
    return static_cast<uint32_t>(static_cast<uint8_t>(bytes[offset])) |
           (static_cast<uint32_t>(static_cast<uint8_t>(bytes[offset + 1])) << 8) |
           (static_cast<uint32_t>(static_cast<uint8_t>(bytes[offset + 2])) << 16) |
           (static_cast<uint32_t>(static_cast<uint8_t>(bytes[offset + 3])) << 24);
}
std::string getCString(const std::string &table, size_t offset) {
    if (offset >= table.size()) fail("string offset outside string table");
    size_t end = table.find('\0', offset);
    if (end == std::string::npos) fail("unterminated string-table entry");
    return table.substr(offset, end - offset);
}
}  // namespace

ObjectFile MachOFormatReader::read(const std::string &bytes) const {
    need(bytes, 0, kMachHeaderSize);
    if (get32(bytes, 0) != kMhMagic) fail("not a 32-bit Mach-O file (bad magic)");

    ObjectFile obj;
    obj.machineType = static_cast<uint16_t>(get32(bytes, 4));
    uint32_t ncmds = get32(bytes, 16);

    uint32_t textOff = 0, textSize = 0, relOff = 0, nreloc = 0;
    uint32_t dataSize = 0;
    uint32_t symOff = 0, nsyms = 0, strOff = 0;
    size_t cmdOff = kMachHeaderSize;
    for (uint32_t i = 0; i < ncmds; i++) {
        need(bytes, cmdOff, 8);
        uint32_t cmd = get32(bytes, cmdOff);
        uint32_t cmdsize = get32(bytes, cmdOff + 4);
        if (cmd == kLcSegment) {
            need(bytes, cmdOff, kSegmentCmdSize);
            uint32_t nsects = get32(bytes, cmdOff + 48);
            if (nsects >= 1) {
                size_t secOff = cmdOff + kSegmentCmdSize;
                need(bytes, secOff, kSectionSize);
                textSize = get32(bytes, secOff + 36);
                textOff = get32(bytes, secOff + 40);
                relOff = get32(bytes, secOff + 48);
                nreloc = get32(bytes, secOff + 52);
            }
            // __bss is a pure size reservation; only its size field (offset 36) matters.
            if (nsects >= 2) {
                size_t bssSecOff = cmdOff + kSegmentCmdSize + kSectionSize;
                need(bytes, bssSecOff, kSectionSize);
                dataSize = get32(bytes, bssSecOff + 36);
            }
        } else if (cmd == kLcSymtab) {
            need(bytes, cmdOff, kSymtabCmdSize);
            symOff = get32(bytes, cmdOff + 8);
            nsyms = get32(bytes, cmdOff + 12);
            strOff = get32(bytes, cmdOff + 16);
        }
        cmdOff += cmdsize;
    }

    need(bytes, textOff, textSize);
    obj.textBytes.assign(bytes.begin() + textOff, bytes.begin() + textOff + textSize);
    obj.dataSize = dataSize;

    std::vector<std::string> symNames(nsyms);
    obj.symbols.reserve(nsyms);
    for (uint32_t i = 0; i < nsyms; i++) {
        size_t base = symOff + static_cast<size_t>(i) * kNlistSize;
        need(bytes, base, kNlistSize);
        uint32_t strx = get32(bytes, base);
        uint8_t type = static_cast<uint8_t>(bytes[base + 4]);
        uint8_t nSect = static_cast<uint8_t>(bytes[base + 5]);
        uint32_t value = get32(bytes, base + 8);
        symNames[i] = getCString(bytes.substr(strOff), strx);
        bool defined = (type & 0x0e) == 0x0e;  // N_SECT
        obj.symbols.push_back({symNames[i], defined ? value : 0, defined, /*isData=*/defined && nSect == 2});
    }

    obj.relocations.reserve(nreloc);
    for (uint32_t i = 0; i < nreloc; i++) {
        size_t base = relOff + static_cast<size_t>(i) * kRelocSize;
        need(bytes, base, kRelocSize);
        uint32_t address = get32(bytes, base);
        uint32_t word = get32(bytes, base + 4);
        uint32_t symIdx = word & 0xFFFFFFu;
        uint16_t type = static_cast<uint16_t>((word >> 28) & 0xF);
        if (symIdx >= symNames.size()) fail("relocation references symbol index out of range");
        obj.relocations.push_back({address, symNames[symIdx], type});
    }

    return obj;
}

}  // namespace rcc
