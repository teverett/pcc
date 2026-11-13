#pragma once

#include "fileformat/FileFormat.h"

namespace rcc {

// `--format=dos33` (link-output only, same group as Raw/Rim/Ptr/A7out/List
// - see FileFormat.h): Apple DOS 3.3's own real "B" (binary) file byte
// stream - a 4-byte header (load address lo/hi, then length lo/hi,
// matching what `BSAVE` itself writes as a file's first 4 bytes) followed
// by `obj.textBytes` verbatim. This is the file's own data, not a disk
// image: DOS 3.3's VTOC/catalog/track-sector-list bookkeeping lives in the
// filesystem, not in this byte stream, so getting this onto a real/
// emulated floppy still needs an existing disk-image tool (e.g.
// AppleCommander's `-p disk.dsk NAME B < this file`) to inject it as a
// catalog entry.
class Dos33FormatWriter : public FileFormatWriter {
   public:
    std::string write(const ObjectFile &obj) const override;
};

class Dos33FormatReader : public FileFormatReader {
   public:
    ObjectFile read(const std::string &bytes) const override;
};

}  // namespace rcc
