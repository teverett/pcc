#pragma once

#include "fileformat/FileFormat.h"

namespace rcc {

// `--format=sna` (link-output only, same group as Raw/Rim/Ptr/A7out/List/Dos33/ProDos/Cmd/
// ColecoRom/Decb/Prg/AtariExe/Atari2600Rom/NesRom - see FileFormat.h): a real 48K ZX Spectrum
// ".sna" snapshot, the de facto standard every Spectrum emulator (Fuse, ZEsarUX, ZXSpin) loads
// directly with no tape-loading step at all - unlike Atari2600RomFormat/NesRomFormat, there's no
// new `--target`/machine ID for this (`--target=z80` already links at this format's own base;
// see docs/platforms.md's own `zx spectrum` entry for why no memory-map hazard like the Atari
// 2600's TIA or ColecoVision's 1KB RAM ceiling exists here - the 48K Spectrum has a full,
// uncontended RAM window at `$4000`-`$FFFF`).
//
// A `.sna` file is always exactly 49179 bytes: a 27-byte header (every Z80 register's own
// value, interrupt mode, border color - see the real, community-documented layout this writer
// follows) followed by a 49152-byte dump of that entire RAM window. Crucially, the header has no
// PC field at all - every real/emulated loader instead restores every other register, sets SP to
// the header's own value, then executes one bare `RET`, which pops the 16-bit value sitting at
// that stack address into PC (the "bootstrap trick" every `.sna`-writing tool uses). This writer
// reserves the last 2 bytes of RAM ($FFFE-$FFFF) for exactly that: `obj.base` is written there,
// and the header's SP points at it, so the loader's `RET` always lands on the real program's own
// entry point. The linked program's own `_start` (see `src/linker/factory/Z80LinkerFactory.cpp`)
// immediately re-seeds SP to its real value as its first instruction, so this bootstrap SP is
// never touched again afterward - but a program's own code/data still can't use those 2 bytes.
//
// Every other header field (register pairs, I/R, interrupt mode, border color) is a fixed,
// reasonable default - this project's own codegen never enables interrupts or reads incoming
// register state, so nothing it generates depends on any of them; IM is set to 1 and the border
// color to 7 (white), matching the real ROM's own post-boot values, purely so a snapshot opened
// without running any of this project's code still looks like an ordinary freshly-booted
// machine.
//
// `obj.base` must be within `$4000`-`$FFFD`, with enough room below `$FFFE` for the whole
// program - there's no bankswitching/paging support (the 48K Spectrum has none to begin with).
class SnaFormatWriter : public FileFormatWriter {
   public:
    std::string write(const ObjectFile &obj) const override;
};

class SnaFormatReader : public FileFormatReader {
   public:
    ObjectFile read(const std::string &bytes) const override;
};

}  // namespace rcc
