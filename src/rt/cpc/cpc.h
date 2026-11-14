#ifndef RCC_RT_CPC_H
#define RCC_RT_CPC_H

// Amstrad CPC (464/664/6128, 64K) runtime for rcc's Z80 backend (--target=z80,
// --format=amsdos - see src/fileformat/amsdos/AmsdosFormat.h's own header comment for why no
// separate target exists: the CPC's own 64KB address space has no memory-map hazard for a
// program loaded in free RAM, the same reasoning docs/platforms.md's own `zx spectrum` entry
// gives).
//
// Programs built with this library link at plain `z80`'s own default base, $8000 (32768):
// comfortably inside the CPC's free RAM window, $4000-$BFFF - clear of the lower ROM's own
// firmware/workspace ($0000-$3FFF, mapped in by default on every real/emulated 64K CPC unless a
// program deliberately pages it out) and of screen memory ($C000-$FFFF).
//
// Every primitive here reaches real, documented CPC hardware directly - no firmware/BIOS call,
// the same "trust only what's fixed across every real machine" stance src/rt/trs80/'s and
// src/rt/coleco/'s own header comments take:
//  - The Gate Array (port $7F) sets ink/border colors and the screen mode via two sequential
//    `OUT`s each - see cpc_set_pen()/cpc_set_mode()'s own comments.
//  - The screen bitmap ($C000-$FFFF, 16KB) is ordinary memory-mapped RAM, like
//    src/rt/trs80/'s own video RAM - no `asm{}` needed for cpc_cls()/cpc_set_pixel().
//  - cpc_psg_write() writes directly to any of the AY-3-8912 sound chip's 14 registers,
//    through the 8255 PPI's own well-documented, consistent write-a-register sequence (see
//    https://www.cpcwiki.eu/index.php/How_to_access_the_PSG_via_PPI) - no tone/envelope/noise
//    helpers built on top of it (same "one raw primitive, no helpers" stance
//    src/rt/zxspectrum/zxspectrum.h's own `zx_set_speaker()` takes, and for the same reason: a
//    real tune needs precisely timed register updates this project's ordinary C codegen can't
//    promise).
//
// Deliberately not covered, each for a documented reason:
//  - Reading the keyboard matrix. The CPC scans its keyboard through the same PSG/PPI bus
//    cpc_psg_write() uses, but the secondary sources consulted while writing this runtime gave
//    inconsistent, conflicting detail on the exact PPI-port-C bit sequence that selects a
//    keyboard row versus a PSG register, and no CPC emulator is available in this environment
//    to cross-check a candidate sequence against. Shipping a plausible-looking but wrong bus
//    sequence risked leaving the PSG in a bad state on real hardware for no real benefit, so
//    this is left unimplemented rather than guessed at.
//  - Modes 0 and 1's pixel-packing (2 and 4 pixels per byte, with each pixel's bits
//    interleaved across the byte's high and low nibbles rather than stored as contiguous
//    fields) - genuinely trickier to get right than mode 2's flat 1-bit-per-pixel layout, and
//    for the same "no emulator to cross-check against" reason as above. `cpc_set_pixel()` below
//    only supports mode 2 (640x200, monochrome); call `cpc_set_mode(2)` first.
//  - Disk access (AMSDOS's own firmware calls) and the CRTC (`$BC00`/`$BD00`) - neither has
//    hardware simple/fixed enough across every real 64K CPC to reach without a firmware call
//    this project doesn't make.
//  - The last 512 bytes of the 16KB screen bitmap, `$FE00`-`$FFFF` (the bottom ~48 of the
//    screen's 200 scanlines in mode 2's own address layout - `cpc_set_pixel()` only guarantees
//    correctness for `y < 152`). This backend's Z80 hardware stack starts at a fixed `$FF00`
//    for every z80 platform (see docs/known-bugs.md's own entry on this) with no way for this
//    runtime to relocate it; writing close enough to `$FF00` while any function is still on the
//    call stack corrupts that call's own return address/locals instead of the intended byte.
//    Confirmed empirically that the real danger starts only a few hundred bytes below `$FF00`
//    (and gets closer with deeper call nesting) - `cpc_cls()`/`cpc_set_pixel()` stop at `$FE00`
//    for a comfortable, if empirical rather than proven, margin above that.

// --- Gate Array (port $7F) ----------------------------------------------

// Sets one of the 16 pens (0-15) or the border (pen 16) to one of the Gate Array's 27 hardware
// color numbers (0-26), via the real two-`OUT` sequence: select the pen, then set its color.
void cpc_set_pen(unsigned char pen, unsigned char color);
// Selects screen mode 0 (160x200, 16 colors), 1 (320x200, 4 colors), or 2 (640x200, 2 colors) -
// the only two bits of the Gate Array's mode/ROM-config register this touches; the lower ROM
// stays paged in either way.
void cpc_set_mode(unsigned char mode);

// --- Screen bitmap ($C000-$FFFF) ----------------------------------------

#define CPC_SCREEN_BASE 49152u  // $C000
// The screen bitmap's last 512 bytes, $FE00-$FFFF, are deliberately left out of both functions
// below - see this file's own header comment for why.
#define CPC_SCREEN_SAFE_BYTES 15872u  // $FE00 - $C000

// Fills the screen bitmap's first 15872 bytes with zero (every pixel at pen 0, regardless of
// mode) - not the full 16KB; see this file's own header comment.
void cpc_cls();
// Sets (`on` != 0) or clears (`on` == 0) one pixel at (x, y), 0 <= x < 640, 0 <= y < 152 - mode
// 2 only (see this file's own header comment for why modes 0/1, and y >= 152, aren't
// supported); a call with y >= 152 is a no-op. Computes the real CPC screen address: each
// character row's own 8 scanlines are 0x800 bytes apart, not contiguous (the same kind of
// interleaving src/rt/zxspectrum/'s own zx_set_pixel() has, with different constants).
void cpc_set_pixel(unsigned int x, unsigned char y, unsigned char on);

// --- Sound (AY-3-8912 PSG, via the 8255 PPI) ------------------------------

// Writes `value` into PSG register `reg` (0-13) directly - the chip's own tone/noise/volume/
// envelope registers; see this file's own header comment for why nothing is built on top of
// this.
void cpc_psg_write(unsigned char reg, unsigned char value);

#endif
