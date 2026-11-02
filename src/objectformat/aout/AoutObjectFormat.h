#pragma once

#include "objectformat/ObjectFormat.h"

namespace pcc {

// `--format=aout` (object mode) - a real, structurally-standard
// classic (V7/BSD) relocatable a.out object file: a real 32-byte
// `struct exec` header (`OMAGIC`), real text relocations
// (`struct relocation_info`) and symbol table (`struct nlist`)
// entries, and the classic a.out string table shape (a leading
// 4-byte total-size field, itself included in that size, followed by
// the NUL-terminated names).
//
// This is a *different* role from ../linker/format/RawWriter.h's own
// `--format=raw` *link*-output (a final, fully-resolved executable
// image with no symbol table at all, padded/signed as a real 8086
// boot sector) - that one only ever consumes an already-linked
// LinkedImage. This one is the pre-link *object*-file shape (real
// symbols and relocations, for a future `--link` step to resolve),
// the a.out-world equivalent of CoffObjectFormat.h's own role; no
// `AoutWriter`-style linked-executable output exists for any of the
// backends that use this object format yet.
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
class AoutObjectFormatWriter : public ObjectFormatWriter {
   public:
    std::string write(const ObjectFile &obj) const override;
};

class AoutObjectFormatReader : public ObjectFormatReader {
   public:
    ObjectFile read(const std::string &bytes) const override;
};

}  // namespace pcc
