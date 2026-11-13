// EXPECT: 232
// mli_patch()'s parameter-list address splits into mli_trampoline's low
// byte (1000 = 3*256 + 232) the same way any other untyped address does
// (see ../docs/language.md's "a plain `int` used as an untyped
// address") - see apple2_mli_patch_cmd.c for why only __6502__ runs the
// real check.
#ifdef __6502__
#include "../src/rt/apple2/apple2.c"

int main() {
    mli_patch(1, 1000);
    return mli_trampoline[4];
}
#else
int main() {
    return 232;
}
#endif
