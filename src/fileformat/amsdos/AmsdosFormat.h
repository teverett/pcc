#pragma once

#include "fileformat/FileFormat.h"

namespace rcc {

// `--format=amsdos` (link-output only, same group as Raw/Rim/Ptr/A7out/List/Dos33/ProDos/Cmd/
// ColecoRom/Decb/Prg/AtariExe/Atari2600Rom/NesRom/Sna - see FileFormat.h): the real 128-byte
// AMSDOS file header every Amstrad CPC disk/tape binary file is prefixed with - the format
// AMSDOS's own `LOAD`/`RUN` and every CPC emulator's "load a binary" convention reads to find a
// program's own load address, length, and entry address, same role TRS-80's `cmd`/CoCo's `decb`
// play for their own machines (see this project's own byte layout confirmed against
// https://www.cpcwiki.eu/index.php/AMSDOS_Header - offsets below match that page exactly).
//
// Byte layout this writer produces (128 bytes total, followed by `obj.textBytes` verbatim - no
// further footer/padding, unlike a fixed-size cartridge format):
//   0        user number - always 0 (every real file uses 0-15; this project has no concept of
//            AMSDOS user areas, so 0 - the default - is the only sensible choice)
//   1-8      filename, space-padded - always "RCC     " (8 chars); AMSDOS's own catalog entry
//            carries the real on-disk filename separately, so this field is cosmetic for any
//            file this writer's own reader round-trips
//   9-11     extension, space-padded - always "BIN"
//   12-15    zero
//   16-17    block number / last block - always 0 (tape-only fields, unused for a disk file)
//   18       file type - always 2 (unprotected binary, version 0: bit 0 clear, bits 1-3 = 1
//            shifted into position, bits 4-7 = 0)
//   19-20    data length (16-bit LE) - `obj.textBytes.size()`
//   21-22    data location (16-bit LE) - `obj.base`
//   23       first block - always $FF (this writer only ever produces a single, complete block)
//   24-25    logical length (16-bit LE) - same as data length above
//   26-27    entry address (16-bit LE) - `obj.base` (this project has no separate "entry point
//            distinct from load address" concept - same convention every other format here uses)
//   28-63    unused, zero
//   64-66    real length (24-bit LE) - same value as the 16-bit data length, zero-extended
//   67-68    checksum (16-bit LE - the real source doesn't state a byte order; little-endian
//            matches every other multi-byte field here and the Z80's own native endianness) -
//            the unsigned sum of bytes 0-66
//   69-127   unused, zero
//
// `obj.textBytes` must be no larger than 65535 bytes (the data-length/logical-length fields are
// 16-bit) - there's no bankswitching/paging concept for a plain binary file to begin with.
class AmsdosFormatWriter : public FileFormatWriter {
   public:
    std::string write(const ObjectFile &obj) const override;
};

class AmsdosFormatReader : public FileFormatReader {
   public:
    ObjectFile read(const std::string &bytes) const override;
};

}  // namespace rcc
