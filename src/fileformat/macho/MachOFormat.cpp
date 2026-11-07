#include "MachOFormat.h"

#include <cstdint>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace pcc {

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

}  // namespace

std::string MachOFormatWriter::write(const ObjectFile &obj) const {
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

}  // namespace pcc
