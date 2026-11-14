// This example only exercises the two pieces of ZX81 hardware simple enough to reach without a
// ROM call - see ../../src/rt/zx81/zx81.h's own header comment for why there's no display/
// bitmap/sound primitive to show here the way ../z80-cpc/ and ../z80-zxspectrum/ have, and for
// the real confidence caveat on both functions below (no official technical reference or
// emulator available to verify either against).
#include "../../src/rt/zx81/zx81.h"

int main() {
    unsigned char key;
    zx81_display_on();
    key = zx81_key_row(5);
    zx81_display_off();
    return key;
}
