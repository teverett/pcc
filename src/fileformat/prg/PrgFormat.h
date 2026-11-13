#pragma once

#include "fileformat/FileFormat.h"

namespace rcc {

// `--format=prg` (link-output only, same group as Raw/Rim/Ptr/A7out/List/Dos33/ProDos/Cmd/
// ColecoRom/Decb - see FileFormat.h): the Commodore "PRG" byte stream every VIC-20/C64/PET
// emulator (VICE) and real KERNAL `LOAD`/`SAVE` read directly - a 2-byte little-endian load
// address, then `obj.textBytes` verbatim. Unlike Dos33Format there's no length field: a PRG
// file's length is just however many bytes follow the header, taken verbatim from the host
// filesystem or disk-image directory entry, not re-stated inside the file. This is the file's
// own data, not a disk/tape image: getting it onto a real/emulated 1541 disk or cassette still
// needs an existing disk/tape-image tool (e.g. VICE's `c1541`) to inject it as a directory or
// tape entry.
class PrgFormatWriter : public FileFormatWriter {
   public:
    std::string write(const ObjectFile &obj) const override;
};

class PrgFormatReader : public FileFormatReader {
   public:
    ObjectFile read(const std::string &bytes) const override;
};

}  // namespace rcc
