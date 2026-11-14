#pragma once

#include "fileformat/FileFormat.h"

namespace rcc {

// `--format=nes` (link-output only, same group as Raw/Rim/Ptr/A7out/List/Dos33/ProDos/Cmd/
// ColecoRom/Decb/Prg/AtariExe/Atari2600Rom - see FileFormat.h): a real iNES ROM image (the de
// facto standard every NES emulator reads), for mapper 0 (NROM) only - a fixed, bankswitching-
// free 32KB PRG-ROM window mapped straight onto the CPU's own $8000-$FFFF, no CHR-ROM (CHR size
// 0 in the header means "the cartridge provides CHR RAM instead", the simplest legal NROM
// configuration since this project drives no PPU graphics - see docs/platforms.md's own `2a03`
// entry for why).
//
// Unlike Atari2600RomFormat (the whole file *is* the address space - no header at all) or
// ColecoRomFormat (the header is itself ordinary cartridge content, mapped like everything
// else), the 16-byte iNES header here is never mapped into the CPU's address space; it only
// configures the emulator/flash-cart loader (signature "NES\x1A", PRG-ROM size in 16KB units,
// CHR-ROM size in 8KB units, two mapper/mirroring flag bytes, 8 reserved zero bytes) ahead of
// the real PRG-ROM payload. `obj.base` must be exactly `$8000` - NROM's only legal load address
// - and `obj.textBytes` is padded with `$FF` up to 32768-6 bytes, then followed by the 6-byte
// NMI/RESET/IRQ vector table (all three pointing at `obj.base`, same "maximally compatible"
// convention Atari2600RomFormat uses - see that header's own comment; a program that actually
// enables NMI, e.g. by writing PPUCTRL's bit 7, would re-enter `_start`, not a dedicated
// handler, since there's no runtime library to provide one).
//
// No bankswitching support (a program over 32762 bytes is rejected outright) and no second,
// larger PRG size - every image produced/accepted here is exactly 16 + 32768 = 32784 bytes.
class NesRomFormatWriter : public FileFormatWriter {
   public:
    std::string write(const ObjectFile &obj) const override;
};

class NesRomFormatReader : public FileFormatReader {
   public:
    ObjectFile read(const std::string &bytes) const override;
};

}  // namespace rcc
