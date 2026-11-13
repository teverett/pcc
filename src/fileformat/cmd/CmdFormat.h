#pragma once

#include "fileformat/FileFormat.h"

namespace rcc {

// `--format=cmd` (link-output only, same group as Raw/Rim/Ptr/A7out/List/Dos33/ProDos - see
// FileFormat.h): TRS-80 DOS's own "/CMD" load-module byte stream (LDOS Quarterly, Vol. 1 No. 4,
// "Roy's Technical Corner"), a sequence of TYPE/LENGTH/DATA records rather than one fixed
// header. A type-1 record is one load-address-prefixed chunk of `obj.textBytes` - up to 256
// code bytes per chunk, matching xtrs's own `hex2cmd`/cmd.c writer - whose LENGTH byte is
// `(chunk size + 2) & 0xFF`: this wraps for the three chunk sizes (254/255/256) whose natural
// LENGTH value would otherwise collide with 0/1/2, and decoding reverses exactly that (a
// LENGTH of 0/1/2 on a type-1 record means 256/257/258, never a literal 0-2-byte data area,
// which would leave no room for the record's own 2-byte load address). A trailing type-2
// record (LENGTH 2, the entry address low-order byte first) ends the file; the entry address
// is always `obj.base`, since every GenericLinker target places its own `_start` at the very
// start of `.text` (see e.g. Z80LinkerFactory.cpp's own `kEntryAsm`). This is the file's own
// data, not a disk image: TRSDOS's GAT/directory bookkeeping lives in the filesystem, not in
// this byte stream, so getting this onto a real/emulated floppy still needs an existing
// disk-image tool to inject it as a directory entry.
class CmdFormatWriter : public FileFormatWriter {
   public:
    std::string write(const ObjectFile &obj) const override;
};

class CmdFormatReader : public FileFormatReader {
   public:
    ObjectFile read(const std::string &bytes) const override;
};

}  // namespace rcc
