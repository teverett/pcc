#include "trs80.h"

// trs80_putchar()/trs80_print()'s own cursor - not touched by the lower-level trs80_putc()/
// trs80_puts() (same split as apple2.c's own COUT-vs-VTAB-vs-direct-poke layering).
int trs80_cursor_row;
int trs80_cursor_col;

void trs80_cls() {
    unsigned char *v;
    int i;
    v = (unsigned char *)TRS80_VIDEO_BASE;
    i = 0;
    while (i < TRS80_COLS * TRS80_ROWS) {
        v[i] = 32;
        i = i + 1;
    }
    trs80_cursor_row = 0;
    trs80_cursor_col = 0;
}

void trs80_putc(int row, int col, unsigned char c) {
    unsigned char *v;
    v = (unsigned char *)TRS80_VIDEO_BASE;
    v[row * TRS80_COLS + col] = c;
}

void trs80_puts(int row, int col, char *s) {
    int i;
    i = 0;
    while (s[i] != 0) {
        trs80_putc(row, col + i, s[i]);
        i = i + 1;
    }
}

void trs80_gotoxy(int row, int col) {
    trs80_cursor_row = row;
    trs80_cursor_col = col;
}

void trs80_scroll() {
    unsigned char *v;
    int i;
    v = (unsigned char *)TRS80_VIDEO_BASE;
    i = 0;
    while (i < TRS80_COLS * (TRS80_ROWS - 1)) {
        v[i] = v[i + TRS80_COLS];
        i = i + 1;
    }
    while (i < TRS80_COLS * TRS80_ROWS) {
        v[i] = 32;
        i = i + 1;
    }
}

void trs80_newline() {
    trs80_cursor_col = 0;
    if (trs80_cursor_row == TRS80_ROWS - 1) {
        trs80_scroll();
    } else {
        trs80_cursor_row = trs80_cursor_row + 1;
    }
}

void trs80_putchar(unsigned char c) {
    if (c == 10) {
        trs80_newline();
        return;
    }
    trs80_putc(trs80_cursor_row, trs80_cursor_col, c);
    trs80_cursor_col = trs80_cursor_col + 1;
    if (trs80_cursor_col >= TRS80_COLS) {
        trs80_newline();
    }
}

void trs80_print(char *s) {
    int i;
    i = 0;
    while (s[i] != 0) {
        trs80_putchar(s[i]);
        i = i + 1;
    }
}

unsigned char trs80_key_row(unsigned char row) {
    unsigned int addr;
    unsigned char *k;
    addr = TRS80_KEYBOARD_BASE | (1u << row);
    k = (unsigned char *)addr;
    return *k;
}

unsigned char trs80_any_key_down() {
    unsigned char row;
    row = 0;
    while (row < 8) {
        if (trs80_key_row(row) != 0) {
            return 1;
        }
        row = row + 1;
    }
    return 0;
}
