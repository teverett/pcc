#include "cpc.h"

// Staging cell for a port I/O instruction's own byte - see cpc.h's own header comment for why
// this can't just be a local/parameter.
unsigned char rt_a;

void cpc_set_pen(unsigned char pen, unsigned char color) {
    if (pen == 16) {
        rt_a = 16;  // bit 4 set: select the border, ignoring bits 3-0
    } else {
        rt_a = pen & 15;
    }
    asm {
        ld a, [g_rt_a]
        out [127], a  ; Gate Array, $7F - select pen (or border)
    }
    rt_a = 64 | (color & 63);
    asm {
        ld a, [g_rt_a]
        out [127], a  ; Gate Array, $7F - set the selected pen's color
    }
}

void cpc_set_mode(unsigned char mode) {
    rt_a = 128 | (mode & 3);
    asm {
        ld a, [g_rt_a]
        out [127], a  ; Gate Array, $7F - screen mode + ROM config
    }
}

void cpc_cls() {
    unsigned char *screen;
    unsigned int i;
    screen = (unsigned char *)CPC_SCREEN_BASE;
    i = 0;
    while (i < CPC_SCREEN_SAFE_BYTES) {
        screen[i] = 0;
        i = i + 1;
    }
}

void cpc_set_pixel(unsigned int x, unsigned char y, unsigned char on) {
    unsigned int addr;
    unsigned int charRow;
    unsigned int line;
    unsigned char *byte;
    unsigned char mask;
    if (y >= 152) return;
    charRow = y >> 3;
    line = y & 7;
    addr = 49152u;
    addr = addr + charRow * 80;
    addr = addr + (line << 11);
    addr = addr + (x >> 3);
    byte = (unsigned char *)addr;
    mask = 128 >> (x & 7);
    if (on) {
        *byte = *byte | mask;
    } else {
        *byte = *byte & (~mask);
    }
}

void cpc_psg_write(unsigned char reg, unsigned char value) {
    rt_a = reg;
    asm {
        ld a, [g_rt_a]
        out [244], a  ; PPI port A, $F4 - the register number
    }
    rt_a = 192;  // BDIR=1,BC1=1 (port C, $F6): select register
    asm {
        ld a, [g_rt_a]
        out [246], a
    }
    rt_a = 0;  // inactive
    asm {
        ld a, [g_rt_a]
        out [246], a
    }
    rt_a = value;
    asm {
        ld a, [g_rt_a]
        out [244], a  ; PPI port A, $F4 - the data byte
    }
    rt_a = 128;  // BDIR=1,BC1=0 (port C, $F6): write register
    asm {
        ld a, [g_rt_a]
        out [246], a
    }
    rt_a = 0;  // inactive
    asm {
        ld a, [g_rt_a]
        out [246], a
    }
}
