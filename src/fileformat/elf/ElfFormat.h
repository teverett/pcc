#pragma once

#include "fileformat/FileFormat.h"

namespace pcc {

// `--format=elf` (compile *or* link) - a real, structurally-standard
// ELF32 relocatable object file (`ET_REL`): a real `Elf32_Ehdr`, a
// real `.text`/`.bss`/`.symtab`/`.strtab`/`.rel.text`/`.shstrtab`
// section-header table (seven sections including the mandatory null
// one), real `Elf32_Sym`/`Elf32_Rel` entries - every field layout
// matches the real ELF32 ABI exactly (confirmed directly against
// `llvm-objdump -h -t -r`, which parses this project's own output
// cleanly). `e_machine` carries this backend's own custom
// `machineType` value directly, written verbatim - the same "custom
// machine ID" stance coff/CoffFormat.h/macho/MachOFormat.h both
// already take (no real EM_* constant exists for any of this
// project's own targets), so a linker (see ../../linker/
// GenericLinker.h) can check it the same way it already checks a
// COFF object's own Machine field. `.bss` is `ObjectFile::dataSize`'s
// own section (SHT_NOBITS - real ELF's own "has a size but no file
// bytes" section type), `0` and empty for every von-Neumann target.
//
// Used identically whether `obj` is a not-yet-linked single-file
// compile (relocations/undefined symbols present) or a TargetLinker's
// own already-linked output (none - see ObjectFile.h's own header
// comment): an ET_REL object with an empty relocation table and every
// symbol defined is still a perfectly valid ET_REL object, just not
// one that needs any further resolving. This replaces the separate,
// simpler PT_LOAD-segment ELF shape `--link --format=elf` used to
// write for 8086 specifically (never a genuinely loadable/runnable
// ELF either way - see this project's own now-removed `ElfWriter.h`'s
// header comment on why that was already true) - one real,
// consistent ELF shape for every use of `--format=elf` instead of
// two.
//
// Only one real ELF-specific limitation, inherent to the format
// itself rather than a deviation this project is choosing: a real
// `Elf32_Rel`'s own `r_info` field reserves only 8 bits for the
// relocation type (`r_info = (symbolIndex << 8) | type`) - every
// relocation `type` value any backend's own assembler uses today
// (pdp7's OR/`+`/`-`, 8086/z80's Abs16/Rel16) already fits in far
// fewer bits than that, so this is a theoretical ceiling only
// (`write()` throws if a future backend's own type value ever
// exceeded it, rather than silently truncating).
class ElfFormatWriter : public FileFormatWriter {
   public:
    std::string write(const ObjectFile &obj) const override;
};

class ElfFormatReader : public FileFormatReader {
   public:
    ObjectFile read(const std::string &bytes) const override;
};

}  // namespace pcc
