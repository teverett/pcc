#ifndef RCC_RT_ZXSPECTRUM_H
#define RCC_RT_ZXSPECTRUM_H

// 48K ZX Spectrum runtime for rcc's Z80 backend (--target=z80, --format=sna - see
// src/fileformat/sna/SnaFormat.h's own header comment for why no separate target exists).
//
// Deliberately not covered, with the real reason in each case:
//  - The ROM's print/character routines (`RST $16`, $0010) and anything else that depends on the
//    ROM's own system variables: a `.sna` snapshot boots straight into user code via the
//    "bootstrap trick" (see SnaFormat.h), never running the ROM's own reset sequence first, so
//    the system variables `RST $16` depends on (current output channel, stream data, etc.) are
//    never actually initialized the way a real boot would leave them - faking a byte-correct
//    copy of that state is substantially more work than this runtime attempts.
//  - Tone/pitch generation on the beeper (port $FE bit 4) and reading the cassette input level
//    (bit 6 of an `IN $FE`): both need a precisely cycle-timed loop - the real hardware only
//    produces a sound/decodes a tape signal through exact toggle timing, which this backend's
//    ordinary C codegen can't promise, the same "racing the beam" problem `examples/
//    6502-atari2600/` and `docs/platforms.md`'s own `atari2600`/`2a03` entries describe.
//    `zx_set_speaker()` below is the one raw, untimed primitive the hardware actually offers;
//    building an audible tone on top of it is left to the caller's own timing.
//  - The Kempston joystick interface (port $1F): a common but genuinely optional third-party
//    add-on, not built into every real 48K Spectrum, unlike the ULA/keyboard/screen below.
//
// Port $FE is shared by the border color (bits 0-2), the MIC output (bit 3), and the speaker
// (bit 4) - and, like every Z80 output port, it can't be read back. `zx_set_border()` and
// `zx_set_speaker()` both go through a shared shadow byte (`zx_ula_shadow`) so setting one never
// clobbers the other's last-written bits, the standard way any real program has to treat this
// port.

// --- Border / speaker (port $FE) ---------------------------------------

// Sets the border to one of the 8 standard colors (0=black .. 7=white; only bits 0-2 of the ULA
// port are meaningful here).
void zx_set_border(unsigned char color);
// Sets (`on` != 0) or clears (`on` == 0) the beeper output bit (bit 4) - a single, untimed
// toggle, not a tone; see this file's own header comment for why pitch generation isn't
// attempted here.
void zx_set_speaker(unsigned char on);

// --- Keyboard (ports $FEFE/$FDFE/.../$7FFE) -----------------------------

// Reads one of the 8 keyboard half-rows raw - row 0 is port $FEFE, row 1 is $FDFE, ..., row 7 is
// $7FFE (every real key's own row/column position is the well-documented "CAPS SHIFT, Z, X, C,
// V" / ... / "0, 9, 8, 7, 6" layout - see any ZX Spectrum reference). Bits 0-4 of the result are
// the row's own 5 keys (0 = pressed, matching the real active-low hardware, unlike
// src/rt/trs80/trs80.c's own trs80_key_row(), whose matrix is active-high); bits 5-7 are always
// set. No ASCII decode - same "undecoded raw bits" convention as trs80_key_row().
unsigned char zx_key_row(unsigned char row);

// --- Screen attributes ($5800-$5AFF) ------------------------------------

#define ZX_ATTR_BASE 22528u  // $5800
#define ZX_ATTR_COLS 32
#define ZX_ATTR_ROWS 24

// Builds an attribute byte from INK/PAPER colors (0-7 each) plus BRIGHT/FLASH flags (0 or 1) -
// the real bit layout every attribute cell uses: FLASH(1) BRIGHT(1) PAPER(3) INK(3).
unsigned char zx_attr(unsigned char ink, unsigned char paper, unsigned char bright, unsigned char flash);
// Writes one attribute cell at (row, col), 0-based - a plain pointer store, since attribute
// memory is ordinary memory-mapped RAM, not a port.
void zx_set_attr(unsigned char row, unsigned char col, unsigned char attr);
// Fills every one of the 768 attribute cells with the same value.
void zx_fill_attrs(unsigned char attr);

// --- Screen bitmap ($4000-$57FF) ----------------------------------------

#define ZX_SCREEN_WIDTH 256
#define ZX_SCREEN_HEIGHT 192

// Sets (ink, `on` != 0) or clears (paper, `on` == 0) the pixel at (x, y) - 0 <= x < 256,
// 0 <= y < 192. Computes the real, famously non-linear ZX Spectrum screen address (each third
// of the screen is stored as 8 interleaved pixel-rows per character row, not a plain linear
// bitmap) rather than a lookup table, so it costs a handful of shifts per call, not a memory
// access pattern a real program would use for anything performance-sensitive.
void zx_set_pixel(unsigned char x, unsigned char y, unsigned char on);
// Clears the whole 6144-byte bitmap to all-paper (every pixel off).
void zx_cls_bitmap();

#endif
