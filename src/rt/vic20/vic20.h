#ifndef RCC_RT_VIC20_H
#define RCC_RT_VIC20_H

// Commodore VIC-20 runtime library for rcc's 6502 backend (--target=6502).
//
// Calls into the VIC-20's KERNAL ROM, the same jump-table entry points
// shared across the whole Commodore 8-bit line (VIC-20/C64/PET/+4/128) -
// unlike the Apple ][ (see src/rt/apple2/apple2.h), there's no separate
// DOS/ProDOS layer to call into, just the KERNAL itself. Every KERNAL
// call here uses a register-based calling convention that has nothing to
// do with rcc's own C calling convention, and rcc's in-process 6502
// assembler only ever encodes the exact mnemonic/operand shapes
// M6502Codegen.cpp itself emits (no LDX/LDY/STX/STY/TAX/TXA/TAY/TYA at all
// - see M6502Assembler.cpp's own header comment), so every wrapper here
// only ever uses the accumulator - a lucky fit, since CHROUT/CHRIN/GETIN
// all take/return their single byte in A. Each one stages its
// argument/result through a global (`rt_a`) immediately before/after an
// `asm { ... }` block, since a C local/parameter has no fixed address an
// asm block's literal text could name directly - only a global does (see
// M6502Codegen.h's own `label()`), same technique as apple2.c's own
// wrappers.
//
// Include this file directly (`#include "vic20.c"`) for a single-file
// `--format=raw` build, or compile vic20.c on its own and `--link` it
// against a separately-compiled program (`--format=coff`/`elf`/`aout`/
// `prg` - see ../../fileformat/prg/PrgFormat.h's own header comment).

// Prints one character via the KERNAL's CHROUT ($FFD2), in PETSCII (not
// ASCII/high-bit-set - unlike apple2.c's a2_cout(), the VIC-20's KERNAL
// doesn't shift display characters into the top half of the byte).
void v20_chrout(unsigned char c);
// Prints a NUL-terminated string one character at a time via v20_chrout().
void v20_puts(char *s);
// Prints a carriage return (PETSCII 13) via v20_chrout(), moving the
// cursor to the start of the next line.
void v20_crout();
// Clears the screen and homes the cursor, via CHROUT of PETSCII 147
// (the "clear screen" control code every KERNAL CHROUT caller, not just
// this library, uses to clear the VIC-20's text screen).
void v20_clrscr();
// Reads and returns one character from the keyboard buffer via GETIN
// ($FFE4), or 0 if the buffer is empty - KERNAL's own non-blocking "poll
// the keyboard" entry point, unlike CHRIN which blocks for a full line.
unsigned char v20_getin();
// Blocks until a key is available and returns it, by polling v20_getin()
// in a loop - KERNAL has no single blocking single-character read of its
// own (CHRIN reads a whole line via the screen editor instead).
unsigned char v20_getch();
// Checks the STOP key via KERNAL's STOP ($FFE1): returns nonzero if it's
// currently pressed. Every long-running loop in a real VIC-20 program
// polls this so RUN/STOP can actually break it, the same role Ctrl-C
// plays on a hosted target.
unsigned char v20_checkstop();

#endif
