// This example only exercises real, documented TI-99/4A hardware: the TMS9918A VDP, reached as
// plain memory addresses (not CRU bits or Z80-style ports) - see ../../src/rt/ti994a/ti994a.h's
// own header comment for exactly what it covers and why `ti_vdp_init_text_mode()` must be
// called first (unlike ColecoVision, a program here never runs through the console's own
// cartridge-boot sequence, so nothing guarantees the VDP starts in any particular mode).
#include "../../src/rt/ti994a/ti994a.h"

int main() {
    ti_vdp_init_text_mode();
    ti_cls();
    ti_puts(10, 9, "HELLO, WORLD!");
    return 0;
}
