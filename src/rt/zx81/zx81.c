#include "zx81.h"

// Staging cell for a port I/O instruction's own byte - see zx81.h's own header comment for why
// this can't just be a local/parameter.
unsigned char rt_a;

void zx81_display_on() {
    rt_a = 0;
    asm {
        ld a, [g_rt_a]
        out [254], a  ; $FE - enables the NMI generator (SLOW mode)
    }
}

void zx81_display_off() {
    rt_a = 0;
    asm {
        ld a, [g_rt_a]
        out [253], a  ; $FD - disables the NMI generator (FAST mode)
    }
}

unsigned char zx81_key_row(unsigned char row) {
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
