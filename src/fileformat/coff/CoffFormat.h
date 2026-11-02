#pragma once

#include "fileformat/FileFormat.h"

namespace pcc {

// A minimal, from-scratch COFF (Common Object File Format) reader/
// writer - one of several interchangeable FileFormatWriter/
// FileFormatReader implementations (see ../FileFormat.h) every
// backend's own in-process assembler can produce a `--format=coff`
// object through, and every TargetLinker's own merge() (see
// ../../linker/Linker.h) can write its own already-linked output
// through, without either ever touching real COFF byte layout
// itself. Not a general COFF reader/writer: exactly what's needed to
// hold this project's own ObjectFile (see ../ObjectFile.h) either
// way - one `.text` section of raw bytes, one `.bss` section (a pure
// size reservation, for a Harvard target's own `ObjectFile::dataSize`
// - `0`, and so empty, for every von-Neumann target), a symbol table
// (defined-vs-external entries, `.text`-vs-`.bss`-resident), and a
// real relocation table (empty once linked - see ObjectFile.h's own
// header comment on what distinguishes a linked ObjectFile).
//
// COFF has no historical tie to any target this project supports
// (PDP-7 predates it entirely; real 16-bit 8086/DOS tooling used OMF,
// not COFF) - this is a deliberate, documented choice, the same
// spirit as other "documented deviation" choices already in this
// project (see docs/pdp-c-compiler.md's "COFF object files"). Every
// object written here uses a custom, non-registered Machine value
// (`ObjectFile::machineType`, set by whichever backend/linker built
// this object - neither 0x1987 nor 0x0086, this project's own two
// examples, is a real COFF machine ID), which existing tooling's COFF
// readers - `objdump` included - reject outright as "not a valid
// object file" purely because of that unrecognized Machine field,
// even though every other field is laid out correctly; confirmed by
// patching a generated file's Machine field to 0x14c
// (IMAGE_FILE_MACHINE_I386) and re-running `objdump -h`/`-t`, which
// then parses it cleanly. Structural correctness is instead verified
// with a small ad-hoc script that parses the format directly (field
// offsets/sizes against this file's own layout) and by a real round
// trip through this project's own linker.
class CoffFormatWriter : public FileFormatWriter {
   public:
    std::string write(const ObjectFile &obj) const override;
};

class CoffFormatReader : public FileFormatReader {
   public:
    ObjectFile read(const std::string &bytes) const override;
};

}  // namespace pcc
