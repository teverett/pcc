#pragma once

#include "fileformat/FileFormat.h"

namespace rcc {

// `--format=a26` (link-output only, same group as Raw/Rim/Ptr/A7out/List/Dos33/ProDos/Cmd/
// ColecoRom/Decb/Prg/AtariExe - see FileFormat.h): a bare Atari 2600 cartridge ROM image - no
// header at all (unlike every other format here), since the hardware itself needs none: the
// 6507 fetches its reset vector directly from the cartridge's own last few bytes. A real 2600
// cartridge is 2048 or 4096 bytes, mapped (bankswitching-free) so its own content repeats to
// fill the 6507's 8KB address space; `obj.base` is expected to be `$F000` for a 4K image or
// `$F800` for a 2K one (see src/linker/factory/M6502LinkerFactory.cpp's own
// `kAtari2600EntryAsm` comment, and M6502Assembler.h's own `kM6502Atari2600BaseAddress`, for
// why $F000/$F800 rather than the real $1000-$1FFF cartridge range) - any other base is
// rejected, since this writer has no way to know which size image an unconventional address is
// meant to produce.
//
// This writer pads `obj.textBytes` with `$FF` up to the chosen size (same convention
// ColecoRomFormat already uses) and writes the three 6507 vectors (NMI, RESET, IRQ - at the
// image's very last six bytes, $FFFA-$FFFF) all pointing at `obj.base` itself: the 2600 never
// really uses NMI/IRQ, but pointing every vector at the same, real entry point is the
// conventional, maximally-compatible choice (matches what every hand-written 2600 "hello
// world" boilerplate does). Getting this onto a real cartridge or a `.bin`/`.a26` file an
// emulator (Stella, etc.) loads directly needs nothing further - this *is* that file, byte for
// byte; no disk/tape-image tool is needed, unlike every other format in this project.
//
// No bankswitching support - a program bigger than 4096 bytes is rejected outright, not
// silently truncated or split across banks.
class Atari2600RomFormatWriter : public FileFormatWriter {
   public:
    std::string write(const ObjectFile &obj) const override;
};

class Atari2600RomFormatReader : public FileFormatReader {
   public:
    ObjectFile read(const std::string &bytes) const override;
};

}  // namespace rcc
