#pragma once

#include "fileformat/FileFormat.h"

namespace pcc {

// ELF32 ET_REL object: Elf32_Ehdr, seven sections (.text/.bss/.symtab/.strtab/.rel.text/.shstrtab
// plus the mandatory null section), Elf32_Sym/Elf32_Rel entries matching the real ELF32 ABI.
// `e_machine` carries the backend's custom machineType verbatim (no real EM_* constant exists for
// these targets). `.bss` is `ObjectFile::dataSize`'s SHT_NOBITS section, empty for von-Neumann
// targets. A linked ObjectFile (empty relocations, all symbols defined) is still a valid ET_REL
// object, just with nothing left to resolve.
// Elf32_Rel's r_info packs the relocation type into its low 8 bits
// (`r_info = (symbolIndex << 8) | type`); `write()` throws if a type exceeds 8 bits.
class ElfFormatWriter : public FileFormatWriter {
   public:
    std::string write(const ObjectFile &obj) const override;
};

class ElfFormatReader : public FileFormatReader {
   public:
    ObjectFile read(const std::string &bytes) const override;
};

}  // namespace pcc
