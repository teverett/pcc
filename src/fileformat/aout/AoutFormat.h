#pragma once

#include "fileformat/FileFormat.h"

namespace pcc {

// `--format=aout` (object mode) - a real, structurally-standard
// classic (V7/BSD) relocatable a.out object file: a real 32-byte
// `struct exec` header (`OMAGIC`), real text relocations
// (`struct relocation_info`) and symbol table (`struct nlist`)
// entries, and the classic a.out string table shape (a leading
// 4-byte total-size field, itself included in that size, followed by
// the NUL-terminated names).
//
// Used identically whether `obj` is a not-yet-linked single-file
// compile (relocations/undefined symbols present) or a TargetLinker's
// own already-linked output (none - see ObjectFile.h's own header
// comment) - `a_entry` carries `obj.base` either way (meaningless,
// left at the default `0`, before linking). This is a genuinely
// different role from raw/RawFormat.h's own `--format=raw` *link*-
// output (a final, fully-resolved executable image with no symbol
// table at all, padded/signed as a real 8086 boot sector) - that
// one's whole point is to drop everything but the raw bytes; this
// one always carries the real symbol/relocation table through.
//
// Two real a.out-specific limitations, inherent to the format
// itself: `a_data`/`a_bss` are always 0 (every backend's own two-pass
// assembler already interleaves code and data into one flat `.text`-
// equivalent, same as every other object format here - see
// ObjectFile.h's own header comment), and a real
// `struct relocation_info`'s own bitfield reserves 24 bits for the
// symbol index and leaves none wider than that for the relocation
// type at all - this writer packs `type` into the 2-bit `r_length`
// field instead (every current value - pdp7's OR/`+`/`-`, 8086/z80's
// Abs16/Rel16 - already fits in 2 bits; `write()` throws if a future
// one ever didn't), leaving the BSD-specific `r_baserel`/`r_jmptable`/
// `r_relative` bits as 0 (this project has no use for any of them).
class AoutFormatWriter : public FileFormatWriter {
   public:
    std::string write(const ObjectFile &obj) const override;
};

class AoutFormatReader : public FileFormatReader {
   public:
    ObjectFile read(const std::string &bytes) const override;
};

}  // namespace pcc
