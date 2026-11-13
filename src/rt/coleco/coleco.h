#ifndef RCC_RT_COLECO_H
#define RCC_RT_COLECO_H

// ColecoVision runtime library for rcc's Z80 backend (--target=z80-coleco).
//
// Unlike src/rt/trs80/ (pure memory-mapped hardware, reachable through a
// plain pointer cast) and src/rt/apple2/ (ROM subroutine calls), every
// piece of ColecoVision hardware this file touches - the TMS9928A VDP,
// the two controller ports, and the SN76489 sound chip - is port-mapped:
// reached only through the Z80's own `IN`/`OUT` instructions (see
// Z80Assembler.h's own header comment for why those needed adding to this
// backend's in-process assembler at all). A C expression can't name a
// port directly, so every primitive here stages its one byte through a
// global (`rt_a`) immediately before/after an `asm { in ... }`/
// `asm { out ... }` block, the same reason src/rt/apple2/apple2.c stages
// through its own `rt_a` before a ROM call - see that file's own header
// comment.
//
// Text output assumes the BIOS has already done what every real
// ColecoVision cartridge actually boots into: Graphics Mode I (32x24
// tiles), with the name table at $1800 and a pattern generator table at
// $0000 already holding a font whose tile index equals the matching
// ASCII code - this file never uploads its own font, only writes name-
// table bytes, matching trs80.c's/apple2.c's own "trust what's already
// there, no byte-identical ROM to verify against" stance (no official
// Coleco technical reference is bundled with this project to confirm the
// exact default addresses byte-for-byte). `coleco_vdp_init_text_mode()`
// is offered for a program that wants to (re)establish this mode itself
// instead of relying on the BIOS leaving it that way; it's unverified
// against real hardware for the same reason.
//
// Not covered at all: sprites, the color table, keypad-mode button
// decoding (only raw, undecoded `IN` values - see coleco_controller_read()
// below), and anything beyond a single `OUT` register write to the sound
// chip (no tone/volume/noise register helpers).

// --- Low-level port primitives --------------------------------------

// Writes one byte to the VDP's data port ($BE) - the next byte at
// whatever address `coleco_vdp_set_write_address()` last set, which then
// auto-increments on the VDP's own side.
void coleco_out_vdp_data(unsigned char value);
// Writes one byte to the VDP's control port ($BF) - either half of a
// register-write or address-set sequence (see the two functions below).
void coleco_out_vdp_ctrl(unsigned char value);
// Reads one byte from the VDP's data port ($BE), from whatever address
// `coleco_vdp_set_read_address()` last set.
unsigned char coleco_in_vdp_data();
// Reads the VDP's status register via its control port ($BF).
unsigned char coleco_in_vdp_ctrl();

// Selects keypad mode (the right fire button and numeric keypad) on both
// controller ports, via a write to the $80-$9F range ($80 itself).
void coleco_out_ctrl_keypad_mode();
// Selects joystick mode (direction + left fire button) on both
// controller ports, via a write to the $C0-$DF range ($C0 itself).
void coleco_out_ctrl_joystick_mode();
// Reads controller 1's raw, undecoded data byte ($FC) - whatever bits
// are set there come back unchanged, same convention as
// src/rt/trs80/trs80.c's own trs80_key_row().
unsigned char coleco_in_ctrl1();
// Reads controller 2's raw, undecoded data byte ($FF).
unsigned char coleco_in_ctrl2();
// Writes one byte directly to the SN76489 sound chip's single write
// port ($FF - the same port number as coleco_in_ctrl2(), a different
// device entirely since reads and writes are independent address
// spaces on the Z80).
void coleco_out_sound(unsigned char value);

// --- VDP register/address sequencing ---------------------------------

// Sets VDP register `reg` (0-7) to `value`, via the two-byte
// value-then-register(|$80) sequence every VDP register write needs.
void coleco_vdp_write_reg(unsigned char reg, unsigned char value);
// Sets the VDP's internal address pointer to `addr` for a following
// coleco_vdp_write_byte() (or several - the pointer auto-increments).
void coleco_vdp_set_write_address(unsigned int addr);
// Same, for a following coleco_vdp_read_byte().
void coleco_vdp_set_read_address(unsigned int addr);
// Writes one byte at the VDP's current (auto-incrementing) address.
void coleco_vdp_write_byte(unsigned char value);
// Reads one byte from the VDP's current (auto-incrementing) address.
unsigned char coleco_vdp_read_byte();

// (Re-)establishes Graphics Mode I with the name table at $1800, the
// color table at $2000, and the pattern generator table at $0000 - the
// layout every function below assumes. See this file's own header
// comment for why a program's own font still has to land in VRAM before
// this is useful for anything but the BIOS's own pre-loaded one.
void coleco_vdp_init_text_mode();

// --- Name-table text output -------------------------------------------

#define COLECO_NAME_TABLE_BASE 6144  // $1800
#define COLECO_COLS 32
#define COLECO_ROWS 24

// Fills the whole 32x24 name table with spaces (tile $20).
void coleco_cls();
// Writes one character at (row, col), 0-based.
void coleco_putc(unsigned char row, unsigned char col, char c);
// Writes a NUL-terminated string starting at (row, col), left to right,
// via one VDP address set and one auto-incrementing write per character
// - no wrapping at the row's own end.
void coleco_puts(unsigned char row, unsigned char col, char *s);

// --- Controllers -------------------------------------------------------

// Selects joystick mode, then returns controller 1's (player == 0) or
// controller 2's (player != 0) raw data byte - undecoded, like
// coleco_in_ctrl1()/coleco_in_ctrl2() above.
unsigned char coleco_controller_read(unsigned char player);

#endif
