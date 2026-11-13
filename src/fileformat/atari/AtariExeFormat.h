#pragma once

#include "fileformat/FileFormat.h"

namespace rcc {

// `--format=atari` (link-output only, same group as Raw/Rim/Ptr/A7out/List/Dos33/ProDos/Cmd/
// ColecoRom/Decb/Prg - see FileFormat.h): the Atari 400/800 DOS binary load file byte stream
// (the real "AUTORUN.SYS"/".xex" convention every Atari DOS loader and emulator, e.g. Altirra,
// reads directly) - a leading 2-byte `$FFFF` marker, then one or more segments, each a 2-byte
// little-endian start address, a 2-byte little-endian *inclusive* end address, then exactly
// `end - start + 1` data bytes. Unlike DecbFormat's blocks, Atari segments don't need to be
// contiguous with each other - that's the whole point of the convention (e.g. loading a
// bootstrap into one address range and the main program into another in the same file).
//
// This writer emits exactly two segments: the program itself (`obj.base`/`obj.textBytes`
// verbatim), then a second, 2-byte segment that pokes the OS's own RUNAD vector ($02E0-$02E1)
// with `obj.base` - the real DOS loader jumps through RUNAD once every segment is loaded, so
// this makes the file directly autorunnable (as `AUTORUN.SYS`) with no separate boot-loader
// stub, rcc's own convention layered on top of the real segment format, not a disk image (same
// relationship ProDosFormat's own footer has to a real ProDOS directory entry). The reader only
// ever reconstructs the *first* segment into the returned ObjectFile (ObjectFile has no way to
// represent more than one (base, bytes) pair) - a RUNAD poke segment this writer's own output
// carries afterward is parsed (to find where it ends) but then discarded, not merged in.
class AtariExeFormatWriter : public FileFormatWriter {
   public:
    std::string write(const ObjectFile &obj) const override;
};

class AtariExeFormatReader : public FileFormatReader {
   public:
    ObjectFile read(const std::string &bytes) const override;
};

}  // namespace rcc
