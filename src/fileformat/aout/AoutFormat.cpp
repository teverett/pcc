#include "AoutFormat.h"

#include <cstdint>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace pcc {

namespace {

[[noreturn]] void fail(const std::string &msg) { throw std::runtime_error("AoutFormat: " + msg); }

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

constexpr uint32_t kOMagic = 0407;  // octal, classic a.out "impure executable/relocatable" magic
constexpr size_t kExecHdrSize = 32;
constexpr size_t kRelocSize = 8;
constexpr size_t kNlistSize = 12;

constexpr uint8_t kNUndf = 0x00, kNText = 0x04, kNBss = 0x08, kNExt = 0x01;

}  // namespace

std::string AoutFormatWriter::write(const ObjectFile &obj) const {
    for (const auto &r : obj.relocations) {
        if (r.type > 3) fail("relocation type " + std::to_string(r.type) + " doesn't fit in a.out's 2-bit r_length");
    }

    // ObjSymbol i is nlist index i directly; no mandatory null entry.
    std::unordered_map<std::string, uint32_t> symIndex;
    for (size_t i = 0; i < obj.symbols.size(); i++) symIndex[obj.symbols[i].name] = static_cast<uint32_t>(i);

    std::string reloctab;
    for (const auto &r : obj.relocations) {
        auto it = symIndex.find(r.symbolName);
        if (it == symIndex.end()) fail("relocation references unknown symbol '" + r.symbolName + "'");
        put32(reloctab, r.offset);  // r_address
        // Bitfield word: r_symbolnum(24) | r_pcrel(1)<<24 | r_length(2)<<25 | r_extern(1)<<27.
        uint32_t word = (it->second & 0xFFFFFFu) | (0u << 24) | ((static_cast<uint32_t>(r.type) & 0x3) << 25) |
                         (1u << 27);
        put32(reloctab, word);
    }

    std::string strtab;
    put32(strtab, 0);  // placeholder; patched to the total length including itself
    std::vector<uint32_t> nameOffsets(obj.symbols.size());
    for (size_t i = 0; i < obj.symbols.size(); i++) {
        nameOffsets[i] = static_cast<uint32_t>(strtab.size());
        strtab += obj.symbols[i].name;
        strtab.push_back('\0');
    }
    uint32_t strtabLen = static_cast<uint32_t>(strtab.size());
    strtab[0] = static_cast<char>(strtabLen & 0xFF);
    strtab[1] = static_cast<char>((strtabLen >> 8) & 0xFF);
    strtab[2] = static_cast<char>((strtabLen >> 16) & 0xFF);
    strtab[3] = static_cast<char>((strtabLen >> 24) & 0xFF);

    std::string symtab;
    for (size_t i = 0; i < obj.symbols.size(); i++) {
        const ObjSymbol &sym = obj.symbols[i];
        put32(symtab, nameOffsets[i]);  // n_strx
        uint8_t nType = !sym.isDefined ? kNUndf : (sym.isData ? kNBss : kNText);
        symtab.push_back(static_cast<char>(nType | kNExt));  // n_type
        symtab.push_back(static_cast<char>(0));                                                      // n_other
        put16(symtab, 0);                                                                             // n_desc
        put32(symtab, sym.isDefined ? sym.value : 0);                                                  // n_value
    }

    std::string out;
    put32(out, kOMagic);
    put32(out, static_cast<uint32_t>(obj.textBytes.size()));  // a_text
    put32(out, 0);                                              // a_data
    put32(out, obj.dataSize);                                    // a_bss
    put32(out, static_cast<uint32_t>(symtab.size()));             // a_syms
    put32(out, obj.base);  // a_entry: absolute execution start address
    put32(out, static_cast<uint32_t>(reloctab.size()));             // a_trsize
    put32(out, 0);                                                   // a_drsize

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

ObjectFile AoutFormatReader::read(const std::string &bytes) const {
    need(bytes, 0, kExecHdrSize);
    uint32_t magic = get32(bytes, 0);
    if (magic != kOMagic) fail("not an OMAGIC a.out file (bad magic)");
    uint32_t aText = get32(bytes, 4);
    uint32_t aBss = get32(bytes, 12);
    uint32_t aSyms = get32(bytes, 16);
    uint32_t aEntry = get32(bytes, 20);
    uint32_t aTrsize = get32(bytes, 24);

    ObjectFile obj;
    obj.machineType = 0;  // a.out's own header has no machine-id field at all
    obj.dataSize = aBss;
    obj.base = aEntry;

    size_t textOff = kExecHdrSize;
    need(bytes, textOff, aText);
    obj.textBytes.assign(bytes.begin() + static_cast<long>(textOff), bytes.begin() + static_cast<long>(textOff + aText));

    size_t relOff = textOff + aText;
    size_t symOff = relOff + aTrsize;
    size_t strOff = symOff + aSyms;
    need(bytes, strOff, 4);
    uint32_t strtabLen = get32(bytes, strOff);
    need(bytes, strOff, strtabLen);
    std::string strtab = bytes.substr(strOff, strtabLen);

    uint32_t numSyms = aSyms / static_cast<uint32_t>(kNlistSize);
    std::vector<std::string> symNames(numSyms);
    obj.symbols.reserve(numSyms);
    for (uint32_t i = 0; i < numSyms; i++) {
        size_t base = symOff + static_cast<size_t>(i) * kNlistSize;
        need(bytes, base, kNlistSize);
        uint32_t strx = get32(bytes, base);
        uint8_t type = static_cast<uint8_t>(bytes[base + 4]);
        uint32_t value = get32(bytes, base + 8);
        symNames[i] = getCString(strtab, strx);
        bool defined = (type & (kNText | kNBss)) != 0;
        obj.symbols.push_back({symNames[i], defined ? value : 0, defined, /*isData=*/(type & kNBss) != 0});
    }

    uint32_t numRelocs = aTrsize / static_cast<uint32_t>(kRelocSize);
    obj.relocations.reserve(numRelocs);
    for (uint32_t i = 0; i < numRelocs; i++) {
        size_t base = relOff + static_cast<size_t>(i) * kRelocSize;
        need(bytes, base, kRelocSize);
        uint32_t address = get32(bytes, base);
        uint32_t word = get32(bytes, base + 4);
        uint32_t symIdx = word & 0xFFFFFFu;
        uint16_t type = static_cast<uint16_t>((word >> 25) & 0x3);
        if (symIdx >= symNames.size()) fail("relocation references symbol index out of range");
        obj.relocations.push_back({address, symNames[symIdx], type});
    }

    return obj;
}

}  // namespace pcc
