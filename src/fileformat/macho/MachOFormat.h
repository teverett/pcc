#pragma once

#include "fileformat/FileFormat.h"

namespace rcc {

// 32-bit Mach-O MH_OBJECT: mach_header, one LC_SEGMENT (__text section),
// one LC_SYMTAB, nlist/relocation_info entries matching the real Mach-O ABI.
// `cputype` carries the backend's custom machineType (no real CPU_TYPE_*
// constant exists for these targets). relocation_info's r_type bitfield is
// only 4 bits; `write()` throws if a type doesn't fit. Relocations are
// always written r_extern=1 (resolved by symbol index) and r_pcrel=0,
// since this writer doesn't know which backend types are pc-relative.
class MachOFormatWriter : public FileFormatWriter {
   public:
    std::string write(const ObjectFile &obj) const override;
};

class MachOFormatReader : public FileFormatReader {
   public:
    ObjectFile read(const std::string &bytes) const override;
};

}  // namespace rcc
