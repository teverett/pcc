// EXPECT: 1
//
// Real Alpha branches (AlphaAssembler.h's Branch format) carry a 21-bit, word-granular
// PC-relative displacement - +-2^20 instructions, not VAX's 127-*byte* conditional-branch
// limit - so there is no encoding this backend ever needs a trampoline for. This instead
// stresses label/branch bookkeeping at a scale a byte-limited target would need one for: an
// `if` skipping, and a `while` looping over, bodies with a few hundred sequential
// statements, in both the taken and not-taken directions, plus a loop with a `break` deep
// inside a long body.
int x;

int main() {
    int i;
    x = 0;
    if (x == 0) {
        x = x + 1; x = x + 1; x = x + 1; x = x + 1; x = x + 1;
        x = x + 1; x = x + 1; x = x + 1; x = x + 1; x = x + 1;
        x = x + 1; x = x + 1; x = x + 1; x = x + 1; x = x + 1;
        x = x + 1; x = x + 1; x = x + 1; x = x + 1; x = x + 1;
        x = x + 1; x = x + 1; x = x + 1; x = x + 1; x = x + 1;
        x = x + 1; x = x + 1; x = x + 1; x = x + 1; x = x + 1;
        x = x + 1; x = x + 1; x = x + 1; x = x + 1; x = x + 1;
        x = x + 1; x = x + 1; x = x + 1; x = x + 1; x = x + 1;
        x = x + 1; x = x + 1; x = x + 1; x = x + 1; x = x + 1;
        x = x + 1; x = x + 1; x = x + 1; x = x + 1; x = x + 1;
    } else {
        x = -1000;
    }
    if (x != 50) return 0;

    if (x != 50) {
        x = -2000; x = -2000; x = -2000; x = -2000; x = -2000;
        x = -2000; x = -2000; x = -2000; x = -2000; x = -2000;
        x = -2000; x = -2000; x = -2000; x = -2000; x = -2000;
        x = -2000; x = -2000; x = -2000; x = -2000; x = -2000;
        x = -2000; x = -2000; x = -2000; x = -2000; x = -2000;
        x = -2000; x = -2000; x = -2000; x = -2000; x = -2000;
        x = -2000; x = -2000; x = -2000; x = -2000; x = -2000;
        x = -2000; x = -2000; x = -2000; x = -2000; x = -2000;
        x = -2000; x = -2000; x = -2000; x = -2000; x = -2000;
        x = -2000; x = -2000; x = -2000; x = -2000; x = -2000;
    } else {
        x = 99;
    }
    if (x != 99) return 0;

    i = 0;
    while (i < 1000) {
        i = i + 1;
        if (i == 300) break;
        x = x + 1; x = x + 1; x = x + 1; x = x + 1; x = x + 1;
        x = x + 1; x = x + 1; x = x + 1; x = x + 1; x = x + 1;
        x = x + 1; x = x + 1; x = x + 1; x = x + 1; x = x + 1;
        x = x + 1; x = x + 1; x = x + 1; x = x + 1; x = x + 1;
    }
    if (i != 300) return 0;
    if (x != 99 + 299 * 20) return 0;
    return 1;
}
