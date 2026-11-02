#pragma once

#include "objectformat/ObjectFormat.h"

namespace pcc {

// `--format=macho` (object mode) - a real, structurally-standard
// 32-bit Mach-O relocatable object file (`MH_OBJECT`): a real
// `mach_header`, one `LC_SEGMENT` load command with one `__text`
// section, one `LC_SYMTAB` load command, real `nlist`/
// `relocation_info` entries - every field layout matches the real
// Mach-O ABI exactly (confirmed directly against `otool -l -s -r`
// and `nm`, both of which parse this project's own output cleanly).
// `cputype` carries this backend's own custom `machineType` value
// directly (no real `CPU_TYPE_*` constant exists for any of this
// project's own targets - the same "custom machine ID" stance
// CoffWriter.h/ElfObjectFormat.h both already take, just in Mach-O's
// own field).
//
// One real Mach-O-specific limitation, inherent to the format itself:
// a real `relocation_info`'s own bitfield packs the relocation type
// into only 4 bits (`r_type`) - every relocation `type` value any
// backend's own assembler uses today already fits in far fewer bits
// than that (`write()` throws if a future one ever didn't, rather
// than silently truncating). Every relocation is written with
// `r_extern=1` (resolved by real symbol-table index, whether that
// symbol is itself defined here or not - simpler and more uniform
// than real Mach-O's own local-vs-external split, which distinguishes
// "relative to a section" from "relative to a symbol" purely as an
// optimization this project has no need for) and `r_pcrel=0` always:
// this writer is deliberately format-neutral (see ObjectFile.h's own
// header comment) and has no idea which backend-specific `type`
// values mean "absolute" vs "relative to the next instruction" (that
// distinction, and the real patch arithmetic for it, lives entirely
// in a linker - see ../linker/GenericLinker.h) - `r_pcrel` is written
// as a harmless constant rather than guessed at.
class MachOObjectFormatWriter : public ObjectFormatWriter {
   public:
    std::string write(const ObjectFile &obj) const override;
};

class MachOObjectFormatReader : public ObjectFormatReader {
   public:
    ObjectFile read(const std::string &bytes) const override;
};

}  // namespace pcc
