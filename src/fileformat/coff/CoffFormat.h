#pragma once

#include "fileformat/FileFormat.h"

namespace rcc {

// Minimal COFF reader/writer, only what's needed to hold an ObjectFile: one `.text` section, one
// `.bss` section (pure size reservation, empty for von-Neumann targets), a symbol table
// (defined-vs-external, `.text`-vs-`.bss`-resident), and a relocation table (empty once linked).
//
// The rcc i386 COFF dialect retains its custom machine ID. Hosted Windows i386 objects use the
// standard IMAGE_FILE_MACHINE_I386 value and standard COFF relocation and local-symbol records.
class CoffFormatWriter : public FileFormatWriter {
   public:
    std::string write(const ObjectFile &obj) const override;
};

class CoffFormatReader : public FileFormatReader {
   public:
    ObjectFile read(const std::string &bytes) const override;
};

}  // namespace rcc
