#include "coco.h"

// coco_putchar()/coco_print()'s own cursor - not touched by the lower-level coco_putc()/
// coco_puts() (same split as trs80.c's own trs80_putc()-vs-trs80_putchar() layering).
int coco_cursor_row;
int coco_cursor_col;

void coco_cls() {
    unsigned char *v;
    int i;
    v = (unsigned char *)COCO_VIDEO_BASE;
    i = 0;
    while (i < COCO_COLS * COCO_ROWS) {
        v[i] = 32;
        i = i + 1;
    }
    coco_cursor_row = 0;
    coco_cursor_col = 0;
}

void coco_putc(int row, int col, unsigned char c) {
    unsigned char *v;
    v = (unsigned char *)COCO_VIDEO_BASE;
    v[row * COCO_COLS + col] = c & 63;
}

void coco_puts(int row, int col, char *s) {
    int i;
    i = 0;
    while (s[i] != 0) {
        coco_putc(row, col + i, s[i]);
        i = i + 1;
    }
}

void coco_gotoxy(int row, int col) {
    coco_cursor_row = row;
    coco_cursor_col = col;
}

void coco_scroll() {
    unsigned char *v;
    int i;
    v = (unsigned char *)COCO_VIDEO_BASE;
    i = 0;
    while (i < COCO_COLS * (COCO_ROWS - 1)) {
        v[i] = v[i + COCO_COLS];
        i = i + 1;
    }
    while (i < COCO_COLS * COCO_ROWS) {
        v[i] = 32;
        i = i + 1;
    }
}

void coco_newline() {
    coco_cursor_col = 0;
    if (coco_cursor_row == COCO_ROWS - 1) {
        coco_scroll();
    } else {
        coco_cursor_row = coco_cursor_row + 1;
    }
}

void coco_putchar(unsigned char c) {
    if (c == 10) {
        coco_newline();
        return;
    }
    coco_putc(coco_cursor_row, coco_cursor_col, c);
    coco_cursor_col = coco_cursor_col + 1;
    if (coco_cursor_col >= COCO_COLS) {
        coco_newline();
    }
}

void coco_print(char *s) {
    int i;
    i = 0;
    while (s[i] != 0) {
        coco_putchar(s[i]);
        i = i + 1;
    }
}
