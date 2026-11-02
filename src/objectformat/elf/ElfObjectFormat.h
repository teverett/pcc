#pragma once

#include "objectformat/ObjectFormat.h"

namespace pcc {

// `--format=elf` (object mode) - a real, structurally-standard ELF32
// relocatable object file (`ET_REL`): a real `Elf32_Ehdr`, a real
// `.text`/`.symtab`/`.strtab`/`.rel.text`/`.shstrtab` section-header
// table (six sections including the mandatory null one), real
// `Elf32_Sym`/`Elf32_Rel` entries - every field layout matches the
// real ELF32 ABI exactly (confirmed directly against
// `llvm-objdump -h -t -r`, which parses this project's own output
// cleanly). `e_machine` carries this backend's own custom
// `machineType` value directly, written verbatim - the same "custom
// machine ID" stance CoffWriter.h/MachOObjectFormat.h both already
// take (no real EM_* constant exists for any of this project's own
// targets), so a linker (see ../../linker/GenericLinker.h) can check
// it the same way it already checks a COFF object's own Machine
// field.
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
class ElfObjectFormatWriter : public ObjectFormatWriter {
   public:
    std::string write(const ObjectFile &obj) const override;
};

class ElfObjectFormatReader : public ObjectFormatReader {
   public:
    ObjectFile read(const std::string &bytes) const override;
};

}  // namespace pcc
