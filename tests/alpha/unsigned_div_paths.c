// EXPECT: 1
//
// Alpha has no integer divide instruction at all (a real, well-known architectural omission -
// see AlphaCodegen.h's own header comment), so both `int` and `long long` division run the
// same bit-serial restoring-division loop (AlphaCodegen.cpp's genDivModCore()), just over 32
// or 64 iterations. This checks the loop's own edge cases for both widths: a divisor with its
// top bit set, a dividend with its top bit set, a divisor exactly 1, equal operands, and a
// dividend smaller than the divisor - each for `/` and `%`, in plain expressions and compound
// assignments.
unsigned int n;
unsigned int d;
unsigned int c;
unsigned long long un;
unsigned long long ud;

int main() {
    n = 4000000000u;
    d = 3u;
    if (n / d != 1333333333u) return 0;
    if (n % d != 1u) return 0;

    d = 2147483649u;  // top bit set
    if (n / d != 1u) return 0;
    if (n % d != 1852516351u) return 0;
    n = 2147483648u;  // top bit set
    d = 4294967295u;
    if (n / d != 0u) return 0;
    if (n % d != 2147483648u) return 0;

    n = 4294967295u;
    d = 1u;
    if (n / d != 4294967295u) return 0;  // would overflow a signed quotient
    if (n % d != 0u) return 0;
    d = 4294967295u;
    if (n / d != 1u) return 0;  // equal operands
    if (n % d != 0u) return 0;
    n = 5u;
    if (n / d != 0u) return 0;  // dividend smaller than divisor
    if (n % d != 5u) return 0;

    c = 4000000000u;
    c /= 7u;
    if (c != 571428571u) return 0;
    c = 4000000000u;
    c %= 7u;
    if (c != 3u) return 0;

    // Literals at or above 2^63 can't be written directly (see docs/known-bugs.md's
    // "Integer literals at or above 2^63 fail to parse" entry, a pre-existing BuildAst.cpp
    // limitation affecting every target); `1ULL << 63` and `~0ULL` build them at runtime
    // instead, exactly as that entry's own workaround suggests.
    un = 9000000000000000000ULL;
    ud = 7ULL;
    if (un / ud != 1285714285714285714ULL) return 0;
    if (un % ud != 2ULL) return 0;

    un = 1ULL << 63;  // top bit set
    ud = ~0ULL;        // 2^64-1
    if (un / ud != 0ULL) return 0;
    if (un % ud != (1ULL << 63)) return 0;

    un = ~0ULL;
    ud = 1ULL;
    if (un / ud != ~0ULL) return 0;  // would overflow a signed quotient
    ud = ~0ULL;
    if (un / ud != 1ULL) return 0;  // equal operands
    if (un % ud != 0ULL) return 0;

    un = 100000000000ULL;
    un = un / 3ULL;
    if (un != 33333333333ULL) return 0;
    return 1;
}
