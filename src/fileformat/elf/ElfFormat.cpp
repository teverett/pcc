#include "ElfFormat.h"

#include <cstdint>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace pcc {

namespace {

[[noreturn]] void fail(const std::string &msg) { throw std::runtime_error("ElfFormat: " + msg); }

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

constexpr size_t kEhdrSize = 52;
constexpr size_t kShdrSize = 40;
constexpr size_t kSymSize = 16;
constexpr size_t kRelSize = 8;

constexpr uint16_t kEtRel = 1;

constexpr uint32_t kShtNull = 0, kShtProgbits = 1, kShtSymtab = 2, kShtStrtab = 3, kShtRel = 9, kShtNobits = 8;
constexpr uint32_t kShfAlloc = 0x2, kShfExecinstr = 0x4, kShfWrite = 0x1;
constexpr uint8_t kSttSection = 3;
constexpr uint16_t kEm386 = 3;
constexpr uint8_t kR386Pc32 = 2;
constexpr uint16_t kPccI386Machine = 0x3386;

bool isPccLocalSymbol(const std::string &name) {
    return name.compare(0, 12, "__pcc_local_") == 0 || name.compare(0, 10, "__pcc_tmp_") == 0;
}

// Fixed section order this writer always produces.
constexpr uint32_t kSecText = 1, kSecBss = 2, kSecSymtab = 3, kSecStrtab = 4, kSecRel = 5,
                    kSecShstrtab = 6;
constexpr uint32_t kNumSections = 7;

}  // namespace

std::string ElfFormatWriter::write(const ObjectFile &obj) const {
    for (const auto &r : obj.relocations) {
        if (r.type > 0xFF) fail("relocation type " + std::to_string(r.type) + " doesn't fit in ELF32_Rel's 8 bits");
    }

    std::string textBytes(obj.textBytes.begin(), obj.textBytes.end());
    if (obj.machineType == kEm386 || obj.machineType == kPccI386Machine) {
        for (const auto &r : obj.relocations) {
            if (r.type != kR386Pc32) continue;
            if (static_cast<size_t>(r.offset) + 4 > textBytes.size()) fail("relocation offset outside .text");
            uint32_t addend = static_cast<uint32_t>(static_cast<uint8_t>(textBytes[r.offset])) |
                              (static_cast<uint32_t>(static_cast<uint8_t>(textBytes[r.offset + 1])) << 8) |
                              (static_cast<uint32_t>(static_cast<uint8_t>(textBytes[r.offset + 2])) << 16) |
                              (static_cast<uint32_t>(static_cast<uint8_t>(textBytes[r.offset + 3])) << 24);
            addend -= 4;
            for (int i = 0; i < 4; ++i) textBytes[r.offset + i] = static_cast<char>(addend >> (i * 8));
        }
    }

    std::string strtab;
    strtab.push_back('\0');
    std::vector<uint32_t> nameOffsets(obj.symbols.size());
    std::unordered_map<std::string, uint32_t> symIndex;
    symIndex[".."] = 1;  // the ELF .text section symbol carries pcc's object-base relocations
    std::vector<size_t> symbolOrder;
    std::vector<size_t> globalSymbols;
    for (size_t i = 0; i < obj.symbols.size(); ++i) {
        const auto &sym = obj.symbols[i];
        if (sym.name == "..") continue;
        if (obj.machineType == kPccI386Machine && isPccLocalSymbol(sym.name)) {
            symbolOrder.push_back(i);
        } else {
            globalSymbols.push_back(i);
        }
    }
    size_t localSymbolCount = symbolOrder.size();
    symbolOrder.insert(symbolOrder.end(), globalSymbols.begin(), globalSymbols.end());
    uint32_t nextSymIndex = 2;
    for (size_t i : symbolOrder) {
        const ObjSymbol &sym = obj.symbols[i];
        nameOffsets[i] = static_cast<uint32_t>(strtab.size());
        strtab += sym.name;
        strtab.push_back('\0');
        symIndex[sym.name] = nextSymIndex++;
    }

    std::string symtab;
    putBytes(symtab, std::string(kSymSize, '\0').data(), kSymSize);  // null entry
    put32(symtab, 0);  // .text section symbol
    put32(symtab, 0);
    put32(symtab, 0);
    symtab.push_back(static_cast<char>(kSttSection));
    symtab.push_back(0);
    put16(symtab, static_cast<uint16_t>(kSecText));
    for (size_t i : symbolOrder) {
        const ObjSymbol &sym = obj.symbols[i];
        put32(symtab, nameOffsets[i]);                                      // st_name
        put32(symtab, sym.value);                                           // st_value
        put32(symtab, 0);                                                   // st_size
        bool isLocal = obj.machineType == kPccI386Machine && isPccLocalSymbol(sym.name);
        symtab.push_back(static_cast<char>((isLocal ? 0 : 1) << 4));        // STB_LOCAL/GLOBAL, STT_NOTYPE
        symtab.push_back(static_cast<char>(0));                             // st_other
        // st_shndx: .text or .bss, whichever address space the symbol lives in, or SHN_UNDEF.
        uint16_t shndx = !sym.isDefined ? 0 : static_cast<uint16_t>(sym.isData ? kSecBss : kSecText);
        put16(symtab, shndx);
    }

    std::string reltab;
    for (const auto &r : obj.relocations) {
        auto it = symIndex.find(r.symbolName);
        if (it == symIndex.end()) fail("relocation references unknown symbol '" + r.symbolName + "'");
        put32(reltab, r.offset);
        put32(reltab, (it->second << 8) | (r.type & 0xFF));  // r_info
    }

    std::string shstrtab;
    std::vector<uint32_t> secNameOffset(kNumSections, 0);
    auto addSecName = [&](uint32_t idx, const char *name) {
        secNameOffset[idx] = static_cast<uint32_t>(shstrtab.size());
        shstrtab += name;
        shstrtab.push_back('\0');
    };
    shstrtab.push_back('\0');  // index 0: empty name, for the null section
    addSecName(kSecText, ".text");
    addSecName(kSecBss, ".bss");
    addSecName(kSecSymtab, ".symtab");
    addSecName(kSecStrtab, ".strtab");
    addSecName(kSecRel, ".rel.text");
    addSecName(kSecShstrtab, ".shstrtab");

    uint32_t textOff = static_cast<uint32_t>(kEhdrSize);
    uint32_t relOff = textOff + static_cast<uint32_t>(obj.textBytes.size());
    uint32_t symOff = relOff + static_cast<uint32_t>(reltab.size());
    uint32_t strOff = symOff + static_cast<uint32_t>(symtab.size());
    uint32_t shstrOff = strOff + static_cast<uint32_t>(strtab.size());
    uint32_t shoff = shstrOff + static_cast<uint32_t>(shstrtab.size());

    std::string out;

    const uint8_t ident[16] = {0x7F, 'E', 'L', 'F', 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    putBytes(out, ident, 16);
    put16(out, kEtRel);
    put16(out, obj.machineType == kPccI386Machine ? kEm386 : obj.machineType);
    put32(out, 1);        // e_version
    put32(out, 0);        // e_entry
    put32(out, 0);        // e_phoff
    put32(out, shoff);    // e_shoff
    put32(out, 0);        // e_flags
    put16(out, static_cast<uint16_t>(kEhdrSize));
    put16(out, 0);        // e_phentsize
    put16(out, 0);        // e_phnum
    put16(out, static_cast<uint16_t>(kShdrSize));
    put16(out, static_cast<uint16_t>(kNumSections));
    put16(out, static_cast<uint16_t>(kSecShstrtab));  // e_shstrndx

    if (!textBytes.empty()) putBytes(out, textBytes.data(), textBytes.size());
    out += reltab;
    out += symtab;
    out += strtab;
    out += shstrtab;

    auto putShdr = [&](uint32_t nameOff, uint32_t type, uint32_t flags, uint32_t offset, uint32_t size,
                        uint32_t link, uint32_t info, uint32_t addralign, uint32_t entsize) {
        put32(out, nameOff);
        put32(out, type);
        put32(out, flags);
        put32(out, 0);  // sh_addr
        put32(out, offset);
        put32(out, size);
        put32(out, link);
        put32(out, info);
        put32(out, addralign);
        put32(out, entsize);
    };
    putShdr(0, kShtNull, 0, 0, 0, 0, 0, 0, 0);  // null section
    putShdr(secNameOffset[kSecText], kShtProgbits, kShfAlloc | kShfExecinstr, textOff,
            static_cast<uint32_t>(obj.textBytes.size()), 0, 0, 1, 0);
    // SHT_NOBITS: sh_size is real but occupies no file space; sh_offset is conventional.
    putShdr(secNameOffset[kSecBss], kShtNobits, kShfAlloc | kShfWrite, textOff + static_cast<uint32_t>(obj.textBytes.size()),
            obj.dataSize, 0, 0, 1, 0);
    putShdr(secNameOffset[kSecSymtab], kShtSymtab, 0, symOff, static_cast<uint32_t>(symtab.size()), kSecStrtab,
            static_cast<uint32_t>(2 + localSymbolCount), 4, static_cast<uint32_t>(kSymSize));
    putShdr(secNameOffset[kSecStrtab], kShtStrtab, 0, strOff, static_cast<uint32_t>(strtab.size()), 0, 0, 1, 0);
    putShdr(secNameOffset[kSecRel], kShtRel, 0, relOff, static_cast<uint32_t>(reltab.size()), kSecSymtab,
            kSecText /*sh_info: section this relocation table applies to*/, 4, static_cast<uint32_t>(kRelSize));
    putShdr(secNameOffset[kSecShstrtab], kShtStrtab, 0, shstrOff, static_cast<uint32_t>(shstrtab.size()), 0, 0, 1, 0);

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

ObjectFile ElfFormatReader::read(const std::string &bytes) const {
    need(bytes, 0, kEhdrSize);
    if (static_cast<uint8_t>(bytes[0]) != 0x7F || bytes[1] != 'E' || bytes[2] != 'L' || bytes[3] != 'F') {
        fail("not an ELF file (bad magic)");
    }
    uint32_t shoff = get32(bytes, 32);
    uint16_t shentsize = get16(bytes, 46);
    uint16_t shnum = get16(bytes, 48);
    if (shentsize != kShdrSize) fail("unexpected section header size " + std::to_string(shentsize));

    // Found by section type, not fixed index, so any valid ET_REL object can be read.
    uint32_t textOff = 0, textSize = 0;
    uint16_t textShndx = 0xFFFF;
    uint32_t dataSize = 0;
    uint32_t symOff = 0, symSize = 0, symLink = 0;
    uint32_t relOff = 0, relSize = 0;
    uint16_t bssShndx = 0xFFFF;  // sentinel: no real section index ever matches this
    bool haveSymtab = false;
    for (uint16_t i = 0; i < shnum; i++) {
        size_t base = shoff + static_cast<size_t>(i) * kShdrSize;
        need(bytes, base, kShdrSize);
        uint32_t type = get32(bytes, base + 4);
        uint32_t offset = get32(bytes, base + 16);
        uint32_t size = get32(bytes, base + 20);
        uint32_t link = get32(bytes, base + 24);
        if (type == kShtProgbits && textSize == 0) {
            textOff = offset;
            textSize = size;
            textShndx = i;
        } else if (type == kShtNobits) {
            dataSize = size;
            bssShndx = i;
        } else if (type == kShtSymtab) {
            symOff = offset;
            symSize = size;
            symLink = link;
            haveSymtab = true;
        } else if (type == kShtRel) {
            relOff = offset;
            relSize = size;
        }
    }
    if (!haveSymtab) fail("no SHT_SYMTAB section found");

    // The string table is whichever section .symtab's sh_link points to.
    size_t strtabBase = symLink * kShdrSize + shoff;
    need(bytes, strtabBase, kShdrSize);
    uint32_t strOff = get32(bytes, strtabBase + 16);
    uint32_t strSize = get32(bytes, strtabBase + 20);
    need(bytes, strOff, strSize);
    std::string strtab = bytes.substr(strOff, strSize);

    ObjectFile obj;
    obj.machineType = get16(bytes, 18);
    if (obj.machineType == kEm386) obj.machineType = kPccI386Machine;
    obj.dataSize = dataSize;
    need(bytes, textOff, textSize);
    obj.textBytes.assign(bytes.begin() + textOff, bytes.begin() + textOff + textSize);

    uint32_t numSyms = symSize / static_cast<uint32_t>(kSymSize);
    std::vector<std::string> symNames(numSyms);
    obj.symbols.reserve(numSyms > 0 ? numSyms - 1 : 0);
    for (uint32_t i = 0; i < numSyms; i++) {
        size_t base = symOff + static_cast<size_t>(i) * kSymSize;
        need(bytes, base, kSymSize);
        uint32_t nameOff = get32(bytes, base);
        uint32_t value = get32(bytes, base + 4);
        uint16_t shndx = get16(bytes, base + 14);
        uint8_t info = static_cast<uint8_t>(bytes[base + 12]);
        symNames[i] = getCString(strtab, nameOff);
        if (i == 0) continue;  // the mandatory null entry
        if (shndx == textShndx && (info & 0x0F) == kSttSection) {
            symNames[i] = "..";
            continue;
        }
        obj.symbols.push_back({symNames[i], value, /*isDefined=*/shndx != 0, /*isData=*/shndx == bssShndx});
    }

    uint32_t numRelocs = relSize / static_cast<uint32_t>(kRelSize);
    obj.relocations.reserve(numRelocs);
    for (uint32_t i = 0; i < numRelocs; i++) {
        size_t base = relOff + static_cast<size_t>(i) * kRelSize;
        need(bytes, base, kRelSize);
        uint32_t offset = get32(bytes, base);
        uint32_t info = get32(bytes, base + 4);
        uint32_t symIdx = info >> 8;
        uint16_t type = static_cast<uint16_t>(info & 0xFF);
        if (symIdx >= numSyms) fail("relocation references symbol index out of range");
        obj.relocations.push_back({offset, symNames[symIdx], type});
    }

    return obj;
}

}  // namespace pcc
