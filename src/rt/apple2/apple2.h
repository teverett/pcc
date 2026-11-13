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
