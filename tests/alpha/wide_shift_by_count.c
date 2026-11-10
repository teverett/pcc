// EXPECT: 1
//
// `long long` shifted by a runtime count (AlphaCodegen.cpp's genBinary()) uses the real
// hardware `sll`/`srl`/`sra` directly - always the 64-bit form, since Alpha has no narrower
// one - with narrowT0To() cleaning up only when the *result*'s own C type is narrower than 64
// bits (never true for `long long` itself). Checks count 0, a count that crosses the 32-bit
// halfway point, and the full-width count 63, for both an unsigned (logical) and a signed
// (arithmetic, sign-filling) shift.
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

    s = s << 1;  // still wide
    if (s != -2048LL) return 0;
    return 1;
}
