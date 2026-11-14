#ifndef RCC_RT_TI994A_H
#define RCC_RT_TI994A_H

// TI-99/4A runtime for rcc's TMS9900 backend (--target=tms9900).
//
// Assumes the real 32K memory expansion is present (the console's own low 8K at $2000-$3FFF
// and high 24K at $A000-$FFFF - without it, the only general RAM on a stock TI-99/4A is the
// 256-byte scratchpad, far too little to run a linked C program at all) - see
// `TMS9900Codegen.h`'s own header comment for why this backend's own code/stack base addresses
// already fall inside that expansion by default, needing no TI-99/4A-specific override.
//
// Unlike every other console runtime in this project (ColecoVision/CPC/Spectrum), nothing here
// reaches hardware through `asm{}` at all: the TMS9918A VDP's four ports are genuine memory
// addresses ($8800 read-data, $8802 read-status, $8C00 write-data, $8C02 write-address/
// register), not CRU bits or Z80-style I/O ports - this backend's full pointer-dereference
// support reaches them directly, the same way `src/rt/trs80/`'s TRS-80 runtime reaches its own
// memory-mapped video RAM, with the real hardware quirk that only the HIGH byte of each 16-bit
// bus cycle is wired to the (8-bit) VDP, matching every other byte-sized operand this backend
// generates.
//
// A linked program here never runs through the console's own GPL interpreter/cartridge-boot
// sequence (this project's own `_start` is the true entry point, not a cartridge header the
// console's ROM jumps to) - so, unlike ColecoVision, there's no guarantee the VDP is in any
// particular mode at all when this code starts running. `ti_vdp_init_text_mode()` must be
// called before any text-output function below, every time - there's no "trust the BIOS left it
// that way" shortcut available here.
//
// Text output assumes Graphics Mode I (the TMS9918A's own 32x24 tile mode, same convention
// `src/rt/coleco/`'s own ColecoVision runtime uses - a closely related VDP, the TMS9928A): name
// table at $1800 (VDP address space, not the Z80/9900's own), pattern generator table at $0000
// with a font whose tile index equals the matching ASCII code. `ti_vdp_init_text_mode()` sets up
// the name/pattern/color table addresses and the mode bits, but uploads no font of its own - no
// official TI technical reference is bundled with this project to confirm the real console ROM's
// own font layout byte-for-byte, so text output is unverified against real hardware for the same
// reason `src/rt/coleco/coleco.h`'s own header comment gives for its own equivalent gap.

// --- Low-level VDP port primitives (memory-mapped, not CRU) -------------

// Writes one byte to the VDP's data port ($8C00) - the next byte at whatever address
// `ti_vdp_set_write_address()` last set, which then auto-increments on the VDP's own side.
void ti_vdp_write_data(unsigned char value);
// Reads one byte from the VDP's data port ($8800), from whatever address
// `ti_vdp_set_read_address()` last set.
unsigned char ti_vdp_read_data();
// Reads the VDP's status register via its own port ($8802).
unsigned char ti_vdp_read_status();
// Writes one byte to the VDP's address/register port ($8C02) - either half of a register-write
// or address-set sequence (see the functions below).
void ti_vdp_write_addr(unsigned char value);

// --- VDP register/address sequencing (same two-byte protocol as the closely related TMS9928A -
// see src/rt/coleco/coleco.h's own header comment) ------------------------

// Sets VDP register `reg` (0-7) to `value`, via the two-byte value-then-register(|$80) sequence
// every VDP register write needs.
void ti_vdp_write_reg(unsigned char reg, unsigned char value);
// Sets the VDP's internal address pointer to `addr` for a following ti_vdp_write_data() (or
// several - the pointer auto-increments).
void ti_vdp_set_write_address(unsigned int addr);
// Same, for a following ti_vdp_read_data().
void ti_vdp_set_read_address(unsigned int addr);

// Establishes Graphics Mode I with the name table at $1800, the color table at $2000, and the
// pattern generator table at $0000 - the layout every function below assumes. Must be called
// before any of them - see this file's own header comment for why there's no "already set up"
// shortcut here.
void ti_vdp_init_text_mode();

// --- Name-table text output -----------------------------------------------

#define TI_NAME_TABLE_BASE 6144u  // $1800
#define TI_COLS 32
#define TI_ROWS 24

// Fills the whole 32x24 name table with spaces (tile $20).
void ti_cls();
// Writes one character at (row, col), 0-based.
void ti_putc(unsigned char row, unsigned char col, char c);
// Writes a NUL-terminated string starting at (row, col), left to right, via one VDP address set
// and one auto-incrementing write per character - no wrapping at the row's own end.
void ti_puts(unsigned char row, unsigned char col, char *s);

#endif
