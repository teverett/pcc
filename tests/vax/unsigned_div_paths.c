// EXPECT: 1
//
// VAX has only a signed divide, so a 32-bit unsigned divide (VaxCodegen.cpp's
// genUnsignedDivMod) splits three ways: a divisor with its top bit set (quotient 0 or
// 1), a dividend below 2^31 (a plain signed divide), and a dividend with its top bit set
// under a small divisor (divide the halved dividend, double, correct the remainder).
// Every path is checked for both `/` and `%`, in plain expressions and compound
// assignments (which swap their operands into place first).
unsigned int n;
unsigned int d;
unsigned int c;

int main() {
    n = 4000000000u;
    d = 3u;
    if (n / d != 1333333333u) return 0;
    if (n % d != 1u) return 0;

    d = 2147483649u;  // top bit set
    if (n / d != 1u) return 0;
    if (n % d != 1852516351u) return 0;
    n = 2147483648u;
    d = 4294967295u;
    if (n / d != 0u) return 0;
    if (n % d != 2147483648u) return 0;

    n = 2147483647u;  // top bit clear
    d = 1000u;
    if (n / d != 2147483u) return 0;
    if (n % d != 647u) return 0;

    n = 4294967295u;
    d = 1u;
    if (n / d != 4294967295u) return 0;  // would overflow a signed quotient
    if (n % d != 0u) return 0;
    d = 4294967295u;
    if (n / d != 1u) return 0;
    if (n % d != 0u) return 0;
    n = 5u;
    if (n / d != 0u) return 0;
    if (n % d != 5u) return 0;

    c = 4000000000u;
    c /= 7u;
    if (c != 571428571u) return 0;
    c = 4000000000u;
    c %= 7u;
    if (c != 3u) return 0;
    c = 4294967295u;
    c /= 2u;
    if (c != 2147483647u) return 0;
    return 1;
}
