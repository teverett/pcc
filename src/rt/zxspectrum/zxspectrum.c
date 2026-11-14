#include "zxspectrum.h"

// Staging cell for a port I/O instruction's own byte - see zxspectrum.h's own header comment for
// why this can't just be a local/parameter.
unsigned char rt_a;

// Port $FE is write-only and shared by border/MIC/speaker (see zxspectrum.h's own header
// comment) - this is the one place either bit actually gets combined before the real OUT.
unsigned char zx_ula_shadow;

void zx_set_border(unsigned char color) {
    zx_ula_shadow = (zx_ula_shadow & 248) | (color & 7);  // keep bits 3-7, replace bits 0-2
    rt_a = zx_ula_shadow;
    asm {
        ld a, [g_rt_a]
        out [254], a  ; ULA port, $FE
    }
}

void zx_set_speaker(unsigned char on) {
    if (on) {
        zx_ula_shadow = zx_ula_shadow | 16;  // bit 4
    } else {
        zx_ula_shadow = zx_ula_shadow & 239;  // ~16
    }
    rt_a = zx_ula_shadow;
    asm {
        ld a, [g_rt_a]
        out [254], a  ; ULA port, $FE
    }
}

unsigned char zx_key_row(unsigned char row) {
    unsigned char rows[8];
    rows[0] = 254;
    rows[1] = 253;
    rows[2] = 251;
    rows[3] = 247;
    rows[4] = 239;
    rows[5] = 223;
    rows[6] = 191;
    rows[7] = 127;
    rt_a = rows[row];
    asm {
        ld a, [g_rt_a]
        in a, [254]  ; port is A:$FE - A already holds the half-row select byte
        ld [g_rt_a], a
    }
    return rt_a;
}

unsigned char zx_attr(unsigned char ink, unsigned char paper, unsigned char bright, unsigned char flash) {
    unsigned char result;
    result = ink & 7;
    result = result | ((paper & 7) << 3);
    if (bright) result = result | 64;
    if (flash) result = result | 128;
    return result;
}

void zx_set_attr(unsigned char row, unsigned char col, unsigned char attr) {
    unsigned char *cells;
    cells = (unsigned char *)ZX_ATTR_BASE;
    cells[row * ZX_ATTR_COLS + col] = attr;
}

void zx_fill_attrs(unsigned char attr) {
    unsigned char *cells;
    int i;
    cells = (unsigned char *)ZX_ATTR_BASE;
    i = 0;
    while (i < ZX_ATTR_COLS * ZX_ATTR_ROWS) {
        cells[i] = attr;
        i = i + 1;
    }
}

void zx_set_pixel(unsigned char x, unsigned char y, unsigned char on) {
    unsigned int addr;
    unsigned int part;
    unsigned char *byte;
    unsigned char mask;
    addr = 16384u;
    part = y & 192;
    addr = addr + (part << 5);
    part = y & 7;
    addr = addr + (part << 8);
    part = y & 56;
    addr = addr + (part << 2);
    addr = addr + (x >> 3);
    byte = (unsigned char *)addr;
    mask = 128 >> (x & 7);
    if (on) {
        *byte = *byte | mask;
    } else {
        *byte = *byte & (~mask);
    }
}

void zx_cls_bitmap() {
    unsigned char *bitmap;
    unsigned int i;
    bitmap = (unsigned char *)16384u;
    i = 0;
    while (i < 6144) {
        bitmap[i] = 0;
        i = i + 1;
    }
}
