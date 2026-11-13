#pragma once

#include "fileformat/FileFormat.h"

namespace rcc {

// `--format=colecorom` (link-output only, same group as Raw/Cmd - see FileFormat.h): the same
// bytes `raw` writes - ColecoLinkerFactory.cpp's own entry object already starts every image with
// the cartridge header (signature + pointer table), see its own header comment - padded with
// 0xFF up to the next size real ColecoVision hardware/emulators expect a cartridge image to come
// in: 8/16/24/32 KiB, the console's own four $8000-$FFFF ROM bank-select sizes per the
// community-documented memory map (no official Coleco technical reference is bundled with this
// project to confirm this byte-for-byte - same "best-effort" caveat as src/rt/trs80/'s own header
// comment). Reading validates the 0x55AA/0xAA55 signature at the very start and that the file is
// one of those four sizes - the two things this format checks that plain `raw` doesn't.
class ColecoRomFormatWriter : public FileFormatWriter {
   public:
    std::string write(const ObjectFile &obj) const override;
};

class ColecoRomFormatReader : public FileFormatReader {
   public:
    ObjectFile read(const std::string &bytes) const override;
};

}  // namespace rcc
