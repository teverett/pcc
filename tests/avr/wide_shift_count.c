// EXPECT: 1
//
// A `long`/`long long` shifted by a runtime count (AVRCodegen.cpp's genShift(), shared between
// every width): a real loop shifting the value one bit at a time, `count` times, through
// `lsr`/`asr` on the top byte and a `ror` chain down through the rest - `isUnsigned` picks the
// fill bit for a right shift. Checks a `long` and a `long long` shifted left/right by a runtime
// count that crosses a byte boundary, an unsigned right shift whose fill must stay zero past the
// sign bit, a signed right shift that must sign-extend, and a shift count of zero.
long la;
long long wa;
unsigned long ula;
unsigned long long uwa;
int n;

int main() {
    la = 1L;
    n = 20;
    if ((la << n) != 1048576L) return 0;

    la = -1024L;
    n = 3;
    if ((la >> n) != -128L) return 0;  // arithmetic: sign-filled

    ula = 2147483648UL;
    n = 4;
    if ((ula >> n) != 134217728UL) return 0;  // logical: no sign fill

    n = 0;
    la = 12345L;
    if ((la << n) != 12345L) return 0;
    if ((la >> n) != 12345L) return 0;

    wa = 1LL;
    n = 40;
    if ((wa << n) != 1099511627776LL) return 0;

    wa = -1LL;
    n = 60;
    if ((wa >> n) != -1LL) return 0;  // all-ones stays all-ones, sign-filled

    uwa = 9223372036854775807ULL;
    n = 32;
    if ((uwa >> n) != 2147483647ULL) return 0;

    return 1;
}
