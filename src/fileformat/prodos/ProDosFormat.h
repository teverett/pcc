#pragma once

#include <cstdint>

#include "fileformat/FileFormat.h"

namespace rcc {

// `--format=prodos` (link-output only, same group as Raw/Rim/Ptr/A7out/
// List - see FileFormat.h): `obj.textBytes` verbatim, plus a trailing
// 3-byte metadata footer (file_type, then aux_type lo/hi) carrying the two
// pieces of ProDOS directory-entry metadata a real volume needs that this
// byte stream otherwise has no room for - aux_type is `obj.base` (ProDOS's
// own convention for a BIN file: aux_type doubles as its BLOAD address).
// This is rcc's own convention, not a standard container format (not
// AppleSingle/AppleDouble): like Dos33Format, the filesystem-level work
// (ProDOS volume directory, block allocation) still needs an existing
// disk-image tool, which this footer's two values feed directly as that
// tool's own file_type/aux_type flags once stripped off.
constexpr uint8_t kProDosFileTypeBin = 0x06;

class ProDosFormatWriter : public FileFormatWriter {
   public:
    std::string write(const ObjectFile &obj) const override;
};

class ProDosFormatReader : public FileFormatReader {
   public:
    ObjectFile read(const std::string &bytes) const override;
};

}  // namespace rcc
