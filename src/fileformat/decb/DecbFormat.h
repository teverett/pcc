#pragma once

#include "fileformat/FileFormat.h"

namespace rcc {

// `--format=decb` (link-output only, same group as Raw/Rim/Ptr/A7out/List/Dos33/ProDos/Cmd/
// ColecoRom - see FileFormat.h): the TRS-80 Color Computer's Disk Extended Color BASIC binary
// byte stream, the real format `LOADM`/`CLOADM` read from disk or cassette, and the format every
// CoCo emulator (XRoar, VCC, MESS/MAME) loads directly. A sequence of blocks:
//  - a data block: type byte 0x00, then a 16-bit big-endian length, a 16-bit big-endian load
//    address, then exactly that many data bytes;
//  - an EOF block: type byte 0xFF, then a 16-bit big-endian length of 0x0000, then a 16-bit
//    big-endian exec (transfer) address.
// Unlike CmdFormat's TRS-80 records, DECB's length field is a full 16 bits, so one data block
// holds this project's whole `.text` with no 256-byte chunking or wraparound to worry about: this
// writer always emits exactly one data block followed by one EOF block, both addressed at
// `obj.base` (every GenericLinker target places `_start` at the very start of `.text` - see e.g.
// M6809LinkerFactory.cpp's own `kEntryAsm`). The reader accepts any number of data blocks, for
// round-trip robustness, even though this writer only ever produces one.
// Like CmdFormat, this is the file's own data, not a disk image: getting it onto a real/emulated
// floppy or cassette still needs an existing disk/cassette-image tool to inject it as a directory
// entry or tape block.
class DecbFormatWriter : public FileFormatWriter {
   public:
    std::string write(const ObjectFile &obj) const override;
};

class DecbFormatReader : public FileFormatReader {
   public:
    ObjectFile read(const std::string &bytes) const override;
};

}  // namespace rcc
