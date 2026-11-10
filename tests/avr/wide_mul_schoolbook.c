// EXPECT: 1
//
// `long`/`long long` multiply (AVRCodegen.cpp's genWideMul()) is schoolbook multiplication
// through hardware `mul`, one partial product per byte pair that can still affect the kept low
// `width` bytes, each one's carry threaded through the rest of the `__wc` accumulator via the
// permanent zero register. Checks a `long` product that overflows 32 bits (silently wraps), one
// that needs carry to propagate across several bytes at once, a negative `long` operand, and a
// `long long` product built from two `long`-sized factors.
long a;
long b;
long long wa;
long long wb;

int main() {
    a = 100000L;
    b = 100000L;
    if (a * b != 1410065408L) return 0;  // 10,000,000,000 mod 2^32

    a = 16777215L;  // 0x00FFFFFF - every low-3-byte partial product contributes
    b = 16L;
    if (a * b != 268435440L) return 0;

    a = -12345L;
    b = 6789L;
    if (a * b != -83810205L) return 0;

    a = 2147483647L;  // INT32_MAX
    b = 2L;
    if (a * b != -2L) return 0;  // wraps to -2 in 32 bits

    wa = 1000000000LL;
    wb = 1000000000LL;
    if (wa * wb != 1000000000000000000LL) return 0;

    wa = -1000000000LL;
    wb = 3000000000LL;
    if (wa * wb != -3000000000000000000LL) return 0;

    return 1;
}
