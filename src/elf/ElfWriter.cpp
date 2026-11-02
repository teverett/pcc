#include "ElfWriter.h"

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

constexpr uint16_t kEtExec = 2;
constexpr uint16_t kEmI386 = 3;      // see ElfWriter.h's own comment on this choice
constexpr uint32_t kEvCurrent = 1;
constexpr uint32_t kPtLoad = 1;
constexpr uint32_t kPfReadWriteExec = 7;  // no code/data split to give separate permissions to - see ElfWriter.h
constexpr uint32_t kEhdrSize = 52;
constexpr uint32_t kPhdrSize = 32;

}  // namespace

std::string writeElfExecutable(uint32_t base, const std::vector<uint8_t> &bytes) {
    std::string out;
    out.reserve(kEhdrSize + kPhdrSize + bytes.size());

    // Elf32_Ehdr.
    out.push_back('\x7f');
    out.push_back('E');
    out.push_back('L');
    out.push_back('F');
    out.push_back(1);  // EI_CLASS = ELFCLASS32
    out.push_back(1);  // EI_DATA = ELFDATA2LSB
    out.push_back(1);  // EI_VERSION = EV_CURRENT
    out.append(9, '\0');  // EI_OSABI, EI_ABIVERSION, EI_PAD (e_ident is 16 bytes total)
    put16(out, kEtExec);
    put16(out, kEmI386);
    put32(out, kEvCurrent);
    put32(out, base);                    // e_entry
    put32(out, kEhdrSize);                // e_phoff - the one Elf32_Phdr follows immediately
    put32(out, 0);                        // e_shoff - no section header table
    put32(out, 0);                        // e_flags
    put16(out, static_cast<uint16_t>(kEhdrSize));
    put16(out, static_cast<uint16_t>(kPhdrSize));
    put16(out, 1);   // e_phnum
    put16(out, 0);   // e_shentsize
    put16(out, 0);   // e_shnum
    put16(out, 0);   // e_shstrndx

    // Elf32_Phdr - one PT_LOAD segment, the whole image.
    put32(out, kPtLoad);
    put32(out, kEhdrSize + kPhdrSize);  // p_offset - segment data starts right after this Phdr
    put32(out, base);                   // p_vaddr
    put32(out, base);                   // p_paddr
    put32(out, static_cast<uint32_t>(bytes.size()));  // p_filesz
    put32(out, static_cast<uint32_t>(bytes.size()));  // p_memsz
    put32(out, kPfReadWriteExec);
    put32(out, 0x1000);  // p_align - conventional page size; nothing here actually needs page alignment

    out.append(bytes.begin(), bytes.end());
    return out;
}

}  // namespace pcc
