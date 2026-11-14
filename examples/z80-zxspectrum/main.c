// A ZX Spectrum ".sna" snapshot boots straight into user code via a
// bootstrap trick (see ../../src/fileformat/sna/SnaFormat.h), never
// running the ROM's own reset sequence first - so there's no real,
// correctly-initialized system-variable state for the ROM's print
// routine (`RST $16`) to depend on, the same reasoning
// ../../src/rt/zxspectrum/zxspectrum.h gives for not attempting one.
// Everything used here instead is real, ROM-free ULA/memory-mapped
// hardware: the border color, a solid attribute fill, and a handful of
// pixels plotted directly into screen memory - all visible on real/
// emulated hardware the instant the snapshot loads.
#include "../../src/rt/zxspectrum/zxspectrum.h"

int main() {
    unsigned char x;
    zx_set_border(5);  // cyan
    zx_fill_attrs(zx_attr(0, 5, 1, 0));  // black ink on bright cyan paper
    zx_cls_bitmap();
    x = 0;
    while (x < 32) {
        zx_set_pixel(100 + x, 96 - x, 1);
        zx_set_pixel(100 + x, 96 + x, 1);
        x = x + 1;
    }
    return 0;
}
