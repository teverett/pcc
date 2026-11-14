#ifndef RCC_RT_ZX81_H
#define RCC_RT_ZX81_H

// ZX81 (1K/16K+) runtime for rcc's Z80 backend (--target=z80, --format=raw).
//
// No separate --format exists for this platform, unlike src/rt/zxspectrum/'s `.sna` or
// src/rt/cpc/'s AMSDOS header - and that's deliberate, not an oversight. Every source consulted
// while writing this runtime describes the ZX81's own native tape/file format (".p") as a
// direct dump of a *saved BASIC program and its workspace*, resumed by re-entering BASIC's own
// continuation logic (recalculating `D_FILE`/`RAMTOP` after loading) - not a general, BASIC-
// independent CPU-state snapshot the way the Spectrum's `.sna` is. Faking the BASIC-continuation
// state well enough for an arbitrary machine-code entry point to work reliably is a different,
// much less certain problem than the "bootstrap trick" `.sna` uses, and no single source agreed
// on the exact header/field layout closely enough to implement with confidence - so this project
// doesn't attempt it. `--format=raw` (plain `z80`'s own existing format) is the only output this
// platform gets.
//
// The ZX81 has no border, no bitmap, no attribute memory, and no sound chip - unlike the
// Spectrum/CPC, there is no memory-mapped or port-mapped hardware that produces any visible or
// audible effect on its own. Its entire display is generated live by the ROM, reading a
// character-code "display file" (`D_FILE`) during precisely-timed CPU windows (`SLOW` mode) -
// there's no pixel/attribute byte this runtime could poke directly, so none is offered.
//
// The two functions below are the only pieces of ZX81 hardware simple enough to reach without a
// ROM call, and even these carry a real confidence caveat the Spectrum/CPC runtimes don't:
// multiple independent sources agree on *what* port `$FE`/`$FD` do, but also that keyboard
// reading from machine code is unreliable outside `SLOW` mode, with no official technical
// reference bundled with this project to verify either claim against (same "trust what's
// documented, no byte-identical reference to confirm it" stance src/rt/coleco/coleco.h's own
// header comment takes, just with less agreement across sources than usual).
//  - `zx81_display_on()`/`zx81_display_off()`: port `$FE`/`$FD` - enables/disables the NMI
//    generator that drives the ROM's own display-generation loop (`SLOW` mode shows the screen
//    while computing; `FAST` mode, the default after `OUT $FD`, runs faster with the display
//    blanked).
//  - `zx81_key_row()`: port `$FE`, read with the row-select byte preloaded into `A` first - the
//    same "port is A:n" hardware quirk src/rt/zxspectrum/'s own `zx_key_row()` uses (see
//    `Z80Assembler.h`'s own header comment). Reported reliable only in `SLOW` mode (call
//    `zx81_display_on()` first) - unverified here either way, with no emulator available that
//    models the ULA's own keyboard-read timing closely enough to check against.

// Enables the NMI generator (port $FE) - resumes `SLOW`-mode display generation.
void zx81_display_on();
// Disables the NMI generator (port $FD) - `FAST` mode: no display, full CPU speed.
void zx81_display_off();
// Reads one of the keyboard matrix's 8 half-rows raw (0-7; the real key layout is the
// Spectrum-like "SHIFT, Z, X, C, V" / ... half-row scheme - see any ZX81 reference). Bits 0-4 of
// the result are that row's own 5 keys (0 = pressed, active-low); bits 5-7 are always set. No
// ASCII decode - same "undecoded raw bits" convention every keyboard-matrix function in this
// project's runtime libraries uses.
unsigned char zx81_key_row(unsigned char row);

#endif
