// This example only uses real, ROM-free Amstrad CPC hardware - see
// ../../src/rt/cpc/cpc.h's own header comment for exactly what that covers (and, just as
// importantly, what it deliberately doesn't, including the last 512 bytes of screen memory,
// which this backend's own fixed hardware-stack convention makes unsafe to write through - see
// docs/known-bugs.md).
#include "../../src/rt/cpc/cpc.h"

int main() {
    unsigned char x;
    cpc_set_mode(2);
    cpc_set_pen(0, 0);   // paper: black
    cpc_set_pen(1, 26);  // ink: bright white
    cpc_set_pen(16, 1);  // border: blue
    cpc_cls();
    x = 0;
    while (x < 32) {
        cpc_set_pixel(100 + x, 76 - x, 1);
        cpc_set_pixel(100 + x, 76 + x, 1);
        x = x + 1;
    }
    return 0;
}
