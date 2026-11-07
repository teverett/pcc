#pragma once

#include "fileformat/FileFormat.h"

namespace pcc {

// Minimal COFF reader/writer, only what's needed to hold an ObjectFile: one `.text` section, one
// `.bss` section (pure size reservation, empty for von-Neumann targets), a symbol table
// (defined-vs-external, `.text`-vs-`.bss`-resident), and a relocation table (empty once linked).
//
// Every object uses a custom, non-registered Machine value (`ObjectFile::machineType`); real COFF
// readers (e.g. objdump) reject this field as invalid even though every other field is laid out
// correctly, so structural correctness is verified with an ad-hoc field-offset script and a round
// trip through this project's own linker instead.
class CoffFormatWriter : public FileFormatWriter {
   public:
    std::string write(const ObjectFile &obj) const override;
};

class CoffFormatReader : public FileFormatReader {
   public:
    ObjectFile read(const std::string &bytes) const override;
};

}  // namespace pcc
