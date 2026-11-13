#pragma once

#include "fileformat/FileFormat.h"

namespace rcc {

// `--format=raw` (compile or link) / `--format=objraw` (compile, inspection-only):
// writes exactly `obj.textBytes`, nothing else — no symbol/relocation table, no header. An
// `objraw` object therefore can't be `--link`ed: the entry sequence calls `main` by name, and
// that symbol was dropped.
// `read()` wraps the whole input back into `textBytes` with empty symbol/relocation tables. No
// magic number, so detectFileFormat() (see ../FileFormat.h) only returns this as the last-resort
// fallback.
class RawFormatWriter : public FileFormatWriter {
   public:
    std::string write(const ObjectFile &obj) const override;
};

class RawFormatReader : public FileFormatReader {
   public:
    ObjectFile read(const std::string &bytes) const override;
};

}  // namespace rcc
