#ifndef RCC_RT_ATARI400_H
#define RCC_RT_ATARI400_H

// Atari 400/800 runtime library for rcc's 6502 backend (--target=6502).
//
// Calls into the Atari OS's CIO (Central I/O) handler via CIOV ($E456),
// writing to/reading from IOCB 0 - the screen editor device ("E:"),
// already open by the OS at boot, so this library never has to OPEN/
// CLOSE it itself. Unlike src/rt/apple2/, src/rt/vic20/, and
// src/rt/c64/'s own ROM/KERNAL calls (every one of which only ever
// touches the accumulator), CIO needs the IOCB number in the X register
// (always 0 here, since only IOCB 0 is used) - the one real new wrinkle
// this target needed: rcc's in-process 6502 assembler had no X-register
// support of any kind until `ldxi` (immediate-only LDX, opcode $A2) was
// added to M6502Assembler.cpp/M65C02Assembler.cpp specifically for this
// (see that file's own `encodeInstr()` comment). Every wrapper below
// still uses only `ldxi 0`, `lda`/`sta`, and `jsr` - no indexed
// addressing, no TAX/TXA, no general X-register support was added.
//
// CIO's own single-character commands (GETCHR=7, PUTCHR=11) pass their
// byte through the accumulator directly, bypassing IOCB 0's buffer-
// address/length fields entirely - the lucky fit that keeps this
// library as simple as the ROM-call ones, once the command byte is
// staged into IOCB 0's own ICCOM cell ($0342) and X is set to select it.
// Each wrapper stages its argument/result through a global (`rt_a`)
// immediately before/after an `asm { ... }` block, since a C local/
// parameter has no fixed address an asm block's literal text could name
// directly - only a global does (see M6502Codegen.h's own `label()`),
// same technique as apple2.c's/vic20.c's/c64.c's own wrappers.
//
// Note on text encoding: unlike apple2.c's ASCII-with-high-bit-set
// convention or vic20.c's/c64.c's PETSCII (which both happen to match
// plain ASCII for ordinary printable text), Atari's own ATASCII differs
// from ASCII for line endings: a line is terminated by ATASCII 155
// ($9B), not a 13/10 CR/LF - see atari_crout() below.
//
// Include this file directly (`#include "atari400.c"`) for a
// single-file `--format=raw` build, or compile atari400.c on its own
// and `--link` it against a separately-compiled program
// (`--format=coff`/`elf`/`aout`/`atari` - see ../../fileformat/atari/
// AtariExeFormat.h's own header comment).

// Prints one character via CIO's PUTCHR command to IOCB 0 ("E:"), in
// ATASCII.
void atari_putch(unsigned char c);
// Prints a NUL-terminated string one character at a time via
// atari_putch().
void atari_puts(char *s);
// Prints an end-of-line (ATASCII 155, not ASCII CR/LF) via
// atari_putch(), moving the cursor to the start of the next line.
void atari_crout();
// Clears the screen and homes the cursor, via atari_putch() of ATASCII
// 125 (the "clear screen" control code).
void atari_clrscr();
// Blocks until a character is available from the keyboard and returns
// it, via CIO's GETCHR command to IOCB 0.
unsigned char atari_getch();

#endif
