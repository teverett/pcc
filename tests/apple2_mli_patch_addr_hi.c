// EXPECT: 3
// Same as apple2_mli_patch_addr_lo.c, for mli_trampoline's high byte
// (1000 = 3*256 + 232).
#ifdef __6502__
#include "../src/rt/apple2/apple2.c"

int main() {
    mli_patch(1, 1000);
    return mli_trampoline[5];
}
#else
int main() {
    return 3;
}
#endif
