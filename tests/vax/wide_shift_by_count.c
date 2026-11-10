// EXPECT: 1
//
// `long long` shifts by a runtime count (VaxCodegen.cpp's genWideShift): `ashq` shifts
// arithmetically, so a logical right shift of an unsigned value clears the sign-filled
// top bits with a mask built from `64 - count`. Count 0 asks for a mask shift of 64, the
// edge case where that mask must come out empty. A set top bit (2^63) makes any
// sign-filling visible.
unsigned long long u;
unsigned long long r;
long long s;
int n;

int main() {
    n = 63;
    u = 1ULL;
    u = u << n;
    u = u + 1ULL;  // 2^63 + 1

    n = 0;
    r = u >> n;
    if (r != u) return 0;
    n = 1;
    r = u >> n;
    if (r != 4611686018427387904ULL) return 0;  // 2^62: no sign fill
    n = 31;
    r = u >> n;
    if (r != 4294967296ULL) return 0;
    n = 32;
    r = u >> n;
    if (r != 2147483648ULL) return 0;
    n = 63;
    r = u >> n;
    if (r != 1ULL) return 0;

    u = 3ULL;
    n = 62;
    r = u << n;
    if (r != (4611686018427387904ULL + 4611686018427387904ULL + 4611686018427387904ULL)) return 0;

    s = -1024LL;
    n = 0;
    if ((s >> n) != -1024LL) return 0;
    n = 3;
    if ((s >> n) != -128LL) return 0;  // arithmetic: sign-filled
    n = 40;
    if ((s >> n) != -1LL) return 0;
    n = 63;
    if ((s >> n) != -1LL) return 0;
    return 1;
}
