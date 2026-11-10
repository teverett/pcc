// EXPECT: 1
//
// `long long` multiply (AlphaCodegen.cpp's genMul()) is a single real `mulq` - a genuine
// 64-bit hardware multiply, unlike every 32-bit-register backend in this project, which has
// to decompose it into cross products. `mulq`'s low 64 bits are exactly a `long long`
// multiply's own result regardless of either operand's sign or overflow, since two's-
// complement multiplication is modular; this exercises that across every sign combination
// and well past 64-bit overflow.
unsigned long long ua;
unsigned long long ub;
long long a;
long long b;

int main() {
    ua = 4294967295ULL;
    ub = 4294967295ULL;
    if (ua * ub != ((4294967294ULL << 32) + 1ULL)) return 0;  // both operands below 2^32

    ua = 2147483648ULL;
    ub = 2ULL;
    if (ua * ub != 4294967296ULL) return 0;

    ua = 3000000000ULL;
    ub = 3ULL;
    if (ua * ub != 9000000000ULL) return 0;
    if (ub * ua != 9000000000ULL) return 0;

    ua = (1ULL << 63) + 1ULL;  // top bit set
    ub = 3ULL;
    if (ua * ub != (3ULL + (1ULL << 63))) return 0;  // overflows past bit 63, low 64 bits only

    a = -3LL;
    b = 7LL;
    if (a * b != -21LL) return 0;
    a = -2147483648LL;
    b = -2LL;
    if (a * b != 4294967296LL) return 0;
    a = 3000000000LL;
    b = -3LL;
    if (a * b != -9000000000LL) return 0;
    a = 123456789012LL;
    b = 1000LL;
    if (a * b != 123456789012000LL) return 0;

    a = -1LL;
    b = -1LL;
    if (a * b != 1LL) return 0;

    a = 1000000007LL;
    b = 999999999LL;
    if (a * b != (1000000007LL * 999999999LL)) return 0;
    return 1;
}
