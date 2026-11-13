#ifndef RCC_RT_COCO_H
#define RCC_RT_COCO_H

// TRS-80 Color Computer (CoCo) runtime for rcc's 6809 backend (--target=6809).
//
// Every function here reaches real CoCo hardware through a fixed, memory-mapped address - video
// RAM for output - rather than a ROM call, the same reasoning src/rt/trs80/trs80.h's own header
// comment gives for the TRS-80: the 6809 backend supports full `T*` pointers, including a plain
// integer literal cast straight to an absolute address, and this project has no byte-identical
// ROM dump to call into anyway.
//
// Video RAM ($0400-$05FF, 1024-1535) is a plain 32-column by 16-row grid with no row
// interleaving, the 6847 VDG's default "internal alphanumeric" text mode on power-up.
//
// UNLIKE the TRS-80 (whose video RAM matches its character generator's own codes directly to
// ASCII - see trs80.h's own header comment), the VDG's internal character set is NOT ASCII: a
// printable uppercase letter, digit, punctuation mark, or space (ASCII 32-95, i.e. $20-$5F) is
// shown correctly only after masking it down to 6 bits (`c & 63`); storing a raw ASCII byte
// above 95, or any lowercase letter (ASCII 97-122), displays the wrong glyph. This runtime
// deliberately only supports that 32-95 range - every coco_putc() store masks with `& 63` - and
// has no lowercase or extended-character support yet; this is a scope limit, not a bug.
//
// No keyboard support yet, matching src/rt/trs80/trs80.h's own scope cut (output only).

#define COCO_VIDEO_BASE 1024u
#define COCO_COLS 32
#define COCO_ROWS 16

// Fills the whole screen with spaces and homes the cursor (row 0, column 0) that
// coco_putchar()/coco_print()/coco_newline() track.
void coco_cls();
// Writes one character at (row, col), both 0-based; no bounds checking. Doesn't touch or move
// the coco_putchar() cursor. Masks `c` with `& 63` per this file's own header comment.
void coco_putc(int row, int col, unsigned char c);
// Writes a NUL-terminated string left to right starting at (row, col), via coco_putc(); does
// not wrap or clip at the screen's own edges, and doesn't touch or move the coco_putchar()
// cursor.
void coco_puts(int row, int col, char *s);

// Moves coco_putchar()'s own cursor to (row, col), both 0-based; no bounds checking.
void coco_gotoxy(int row, int col);
// Scrolls the whole screen up by one row (row 0's own contents are lost), via coco_putc(), and
// fills the newly-exposed bottom row with spaces. Doesn't move the cursor.
void coco_scroll();
// Moves the cursor to column 0 of the next row, scrolling via coco_scroll() first if the cursor
// was already on the bottom row.
void coco_newline();
// Writes one character at the cursor and advances it one column, via coco_putc(); a '\n'
// instead calls coco_newline() and writes nothing. Wraps to the next row (scrolling if needed,
// via coco_newline()) once the cursor would otherwise run past the last column.
void coco_putchar(unsigned char c);
// Writes a NUL-terminated string via coco_putchar(), one character at a time.
void coco_print(char *s);

#endif
