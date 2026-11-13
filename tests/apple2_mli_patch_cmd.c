// EXPECT: 101
// mli_patch() stages a ProDOS MLI call's command byte into
// mli_trampoline without jumping anywhere (see
// ../src/rt/apple2/apple2.h's own header comment), so it's checkable on
// a bare 6502 core with no real Apple ][ ROM loaded. Every other target
// just returns the same expected value directly - this exercises
// 6502-specific hardware wiring (../src/rt/apple2/apple2.c's own
// `asm { }` blocks), not a portable language feature.
#ifdef __6502__
#include "../src/rt/apple2/apple2.c"

int main() {
    mli_patch(101, 1000);
    return mli_trampoline[3];
}
#else
int main() {
    return 101;
}
#endif
