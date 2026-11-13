#ifndef RCC_RT_C64_H
#define RCC_RT_C64_H

// Commodore 64 runtime library for rcc's 6502 backend (--target=6502).
//
// Calls into the C64's KERNAL ROM - the same jump-table entry points as
// src/rt/vic20/vic20.h's own VIC-20 library (CHROUT/GETIN/STOP/CHRIN all
// sit at the identical addresses on every Commodore 8-bit machine that
// shares this KERNAL lineage), duplicated here under its own name rather
// than shared, matching this project's own one-directory-per-real-machine
// convention (see src/rt/apple2/, src/rt/trs80/, src/rt/coco/ - each its
// own directory even where two machines' ROM calls would otherwise
// coincide). Every KERNAL call here uses a register-based calling
// convention that has nothing to do with rcc's own C calling convention,
// and rcc's in-process 6502 assembler only ever encodes the exact
// mnemonic/operand shapes M6502Codegen.cpp itself emits (no LDX/LDY/STX/
// STY/TAX/TXA/TAY/TYA at all - see M6502Assembler.cpp's own header
// comment), so every wrapper here only ever uses the accumulator - a
// lucky fit, since CHROUT/CHRIN/GETIN all take/return their single byte
// in A. Each one stages its argument/result through a global (`rt_a`)
// immediately before/after an `asm { ... }` block, since a C local/
// parameter has no fixed address an asm block's literal text could name
// directly - only a global does (see M6502Codegen.h's own `label()`),
// same technique as apple2.c's/vic20.c's own wrappers.
//
// Known risk, not yet verified against real hardware (same caveat as
// src/rt/apple2/'s own "frame-storage vs. firmware memory map" note):
// rcc's 6502 codegen hardcodes `kFrameStackBase = $0200-$05FF` as its
// frame bump allocator (M6502Codegen.cpp), which overlaps the C64
// KERNAL's own keyboard queue (`KEYD`, $0277-$0280) and its count byte
// (`NDX`, $00C6, outside this range) - a program with a deep enough call
// stack *and* outstanding buffered keystrokes could in principle have
// one overwrite the other. None of this library's own functions build a
// deep enough frame to trigger it in practice (every wrapper below is a
// single stack frame around one `jsr`), but a caller doing heavy
// recursion while also calling v20_getin()/c64_getin() repeatedly should
// be aware of it.
//
// Include this file directly (`#include "c64.c"`) for a single-file
// `--format=raw` build, or compile c64.c on its own and `--link` it
// against a separately-compiled program (`--format=coff`/`elf`/`aout`/
// `prg` - see ../../fileformat/prg/PrgFormat.h's own header comment).

// Prints one character via the KERNAL's CHROUT ($FFD2), in PETSCII (not
// ASCII/high-bit-set - like src/rt/vic20/vic20.c's own v20_chrout(), the
// KERNAL doesn't shift display characters into the top half of the byte).
void c64_chrout(unsigned char c);
// Prints a NUL-terminated string one character at a time via c64_chrout().
void c64_puts(char *s);
// Prints a carriage return (PETSCII 13) via c64_chrout(), moving the
// cursor to the start of the next line.
void c64_crout();
// Clears the screen and homes the cursor, via CHROUT of PETSCII 147 (the
// "clear screen" control code every KERNAL CHROUT caller uses).
void c64_clrscr();
// Reads and returns one character from the keyboard buffer via GETIN
// ($FFE4), or 0 if the buffer is empty - KERNAL's own non-blocking "poll
// the keyboard" entry point, unlike CHRIN which blocks for a full line.
unsigned char c64_getin();
// Blocks until a key is available and returns it, by polling c64_getin()
// in a loop - KERNAL has no single blocking single-character read of its
// own (CHRIN reads a whole line via the screen editor instead).
unsigned char c64_getch();
// Checks the STOP key via KERNAL's STOP ($FFE1): returns nonzero if it's
// currently pressed. Every long-running loop in a real C64 program polls
// this so RUN/STOP can actually break it, the same role Ctrl-C plays on
// a hosted target.
unsigned char c64_checkstop();

#endif
