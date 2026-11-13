#ifndef RCC_RT_APPLE2_H
#define RCC_RT_APPLE2_H

// Apple ][ runtime library for rcc's 6502 backend (--target=6502).
//
// Calls into the Apple ][ monitor ROM and the ProDOS MLI. Both use a
// register-based calling convention (arguments/results in the 6502's A
// register, or - for the MLI - inline bytes immediately after the call
// instruction) that has nothing to do with rcc's own C calling convention,
// and rcc's in-process 6502 assembler only ever encodes the exact
// mnemonic/operand shapes M6502Codegen.cpp itself emits (no LDX/LDY/STX/STY/
// TAX/TXA/TAY/TYA at all - see M6502Assembler.cpp's own header comment), so
// every wrapper here only ever uses the accumulator. Each one stages its
// argument/result through a global (`rt_a`) immediately before/after an
// `asm { ... }` block, since a C local/parameter has no fixed address an
// asm block's literal text could name directly - only a global does (see
// M6502Codegen.h's own `label()`).
//
// Include this file directly (`#include "apple2.c"`) for a single-file
// `--format=raw` build, or compile apple2.c on its own and `--link` it
// against a separately-compiled program (`--format=coff`/`elf`/`aout`).

// Prints one character via the monitor ROM's COUT ($FDED).
void a2_cout(unsigned char c);
// Prints a NUL-terminated string one character at a time via a2_cout().
void a2_puts(char *s);
// Moves the cursor to the start of the next line, via CROUT ($FD8E).
void a2_crout();
// Waits for and returns one keystroke (high bit set), via RDKEY ($FD0C).
unsigned char a2_rdkey();
// Clears the text screen and homes the cursor, via HOME ($FC58).
void a2_home();
// Prints a byte as two hex digits, via PRBYTE ($FDDA).
void a2_prbyte(unsigned char b);
// Prints a nibble (low 4 bits of `n`) as one hex digit, via PRHEX ($FDE3).
void a2_prhex(unsigned char n);
// Rings the bell, via BELL ($FF3A).
void a2_bell();
// Busy-waits roughly `n` * the monitor's own WAIT ($FCA8) delay unit.
void a2_wait(unsigned char n);
// Prints one character directly to the screen, bypassing the $36/$37
// output hook, via COUT1 ($FDF0). Same high-bit convention as a2_cout().
void a2_cout1(unsigned char c);
// Waits for and returns one keystroke, with RDKEY's own escape-sequence
// handling, via RDCHAR ($FD35).
unsigned char a2_rdchar();
// Reads one key directly from the keyboard hardware, via KEYIN ($FD1B).
unsigned char a2_keyin();
// Moves the cursor to text row `row` (0-based), via VTAB ($FC22).
void a2_vtab(unsigned char row);
// Clears from the cursor to the end of the current line, via CLREOL ($FC9C).
void a2_clreol();
// Clears from the cursor to the end of the text page, via CLREOP ($FC42).
void a2_clreop();
// Sets the soft switches for 40-column text mode, via SETTXT ($FB39).
void a2_settxt();
// Sets the soft switches for lo-res graphics mode, via SETGR ($FB40).
void a2_setgr();
// Sets inverse video text, via SETINV ($FE80).
void a2_setinv();
// Sets normal video text, via SETNORM ($FE84).
void a2_setnorm();
// Resets the character-input hook ($38/$39) to the ROM keyboard routine,
// via SETKBD ($FE89).
void a2_setkbd();
// Resets the character-output hook ($36/$37) to the ROM screen routine,
// via SETVID ($FE93).
void a2_setvid();
// Clears the lo-res graphics screen to black, via CLRSCR ($F832).
void a2_grclrscr();
// Clears the top (non-text-window) part of the lo-res graphics screen to
// black, via CLRTOP ($F836).
void a2_clrtop();

// Reads one line of keyboard input via GETLN ($FD6A): shows the current
// PROMPT ($33) character, echoes keystrokes, and supports the usual
// line-editing keys. The monitor itself (not this wrapper, and not rcc -
// see docs/known-bugs.md's "'@' addr pinning isn't implemented for the
// 6502/65c02 backends' global arrays") always writes the finished line
// into its own fixed buffer at $200-$2FF, one byte per character with bit
// 7 set (the same normal-text convention as a2_cout()'s own input),
// terminated by a single $8D (a carriage return, not 0) rather than NUL.
void a2_getln();

// Copies the inclusive byte range [from, to] to a same-size range starting
// at dest, via MOVE ($FE2C). `from`/`to`/`dest` are plain untyped addresses
// (see docs/language.md's "a plain `int` used as an untyped address"), not
// typed pointers - MOVE stages them through the monitor's own zero-page
// cells ($3C-$3F, $42-$43) rather than any 6502 register.
void a2_move(int from, int to, int dest);

// Patches mli_trampoline's command byte and parameter-list address (an
// untyped address - see docs/language.md's "a plain `int` used as an
// untyped address") without calling it - the piece that's testable without
// a real/emulated Apple ][ ROM, since nothing here jumps anywhere.
void mli_patch(unsigned char cmd, int paramsAddr);
// Issues one ProDOS MLI call (JSR $BF00, cmd byte, parameter-list address -
// see ProDOS Technical Reference Manual ch. 5) and returns its error code
// (0 = no error, matching the accumulator ProDOS itself leaves on return).
int mli_call(unsigned char cmd, int paramsAddr);

#endif
