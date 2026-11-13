#ifndef RCC_RT_TRS80_H
#define RCC_RT_TRS80_H

// TRS-80 Model I/III runtime for rcc's Z80 backend (--target=z80).
//
// Every function here reaches real TRS-80 hardware through a fixed, memory-mapped address -
// video RAM for output, the keyboard matrix for input - rather than a ROM call: the Z80 backend
// supports full `T*` pointers (see docs/language.md), including a plain integer literal cast
// straight to an absolute address, so none of apple2.c's `rt_a`-staging-global workaround for
// `asm { }` blocks is needed here. This also sidesteps the real obstacle a ROM-call runtime
// would hit: Model I/III/4 ROMs put the same logical routine (CLS, keyboard-to-ASCII, ...) at
// different entry points, and this project has no byte-identical dump of any of them to test
// against - every function below instead only depends on the one piece of TRS-80 hardware that
// stayed fixed across all three machines (video RAM at $3C00, keyboard matrix at $3800).
//
// Video RAM ($3C00-$3FFF, 15360-16383) is a plain 64-column by 16-row grid with no row
// interleaving (unlike the Apple ][ - see src/rt/apple2/apple2.h's own header comment).
// Printable ASCII ($20-$7E) matches the TRS-80 character generator's own codes directly for the
// characters this runtime ever writes.
//
// The keyboard matrix ($3800-$38FF) has no ASCII decode here, deliberately: reading address
// $3800 | (1 << row) returns an 8-bit mask of which of that row's own columns are currently held
// down, but which column bit means which physical key (beyond rows 0-5's well-documented
// "@ A B C D E F G" / ... / "8 9 * : ; , - ." letter/digit/punctuation layout) is exactly the
// kind of thing that differs between Model I and Model III (SHIFT/ENTER/arrow keys live in
// different rows and bit positions on the two machines) - see trs80_key_row()'s own comment.
// Build any higher-level "read one key" helper on top of trs80_key_row()/trs80_any_key_down()
// for the specific machine/key layout actually being targeted.

#define TRS80_VIDEO_BASE 15360u
#define TRS80_COLS 64
#define TRS80_ROWS 16
#define TRS80_KEYBOARD_BASE 14336u

// Fills the whole screen with spaces and homes the cursor (row 0, column 0) that
// trs80_putchar()/trs80_print()/trs80_newline() track.
void trs80_cls();
// Writes one character at (row, col), both 0-based; no bounds checking. Doesn't touch or move
// the trs80_putchar() cursor.
void trs80_putc(int row, int col, unsigned char c);
// Writes a NUL-terminated string left to right starting at (row, col), via trs80_putc(); does
// not wrap or clip at the screen's own edges, and doesn't touch or move the trs80_putchar()
// cursor.
void trs80_puts(int row, int col, char *s);

// Moves trs80_putchar()'s own cursor to (row, col), both 0-based; no bounds checking.
void trs80_gotoxy(int row, int col);
// Scrolls the whole screen up by one row (row 0's own contents are lost), via trs80_putc(), and
// fills the newly-exposed bottom row with spaces. Doesn't move the cursor.
void trs80_scroll();
// Moves the cursor to column 0 of the next row, scrolling via trs80_scroll() first if the
// cursor was already on the bottom row.
void trs80_newline();
// Writes one character at the cursor and advances it one column, via trs80_putc(); a '\n'
// instead calls trs80_newline() and writes nothing. Wraps to the next row (scrolling if needed,
// via trs80_newline()) once the cursor would otherwise run past the last column.
void trs80_putchar(unsigned char c);
// Writes a NUL-terminated string via trs80_putchar(), one character at a time.
void trs80_print(char *s);

// Reads the keyboard matrix row selected by `row` (0-7, picking address
// TRS80_KEYBOARD_BASE | (1 << row)): bit `col` of the result is set while that row/column's own
// key is held down. No ASCII decode - see this file's own header comment for why.
unsigned char trs80_key_row(unsigned char row);
// Scans every one of the keyboard matrix's 8 rows via trs80_key_row() and returns 1 if any key
// anywhere is currently held down, 0 otherwise - enough for a "press any key" wait with no
// per-key decode needed.
unsigned char trs80_any_key_down();

#endif
