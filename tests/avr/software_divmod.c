// EXPECT: 1
//
// AVR has no hardware divide at all (unlike targets with at least a signed one) - every `/`/`%`
// here is AVRCodegen.cpp's own bit-serial restoring-division loop (genDivMod()), unsigned only,
// wrapped with a negate-magnitudes/fix-up-the-sign dance for a signed operand. Checks a plain
// unsigned divide/mod, a divisor whose top bit is set (the largest case the loop's comparison
// chain has to get right), every sign combination of a signed divide/mod, compound `/=`/`%=`,
// and the `long` (32-bit) path through the same loop at a different width.
unsigned int un;
unsigned int ud;
int n;
int d;
long ln;
long ld;

int main() {
    un = 100u;
    ud = 7u;
    if (un / ud != 14u) return 0;
    if (un % ud != 2u) return 0;

    un = 65535u;
    ud = 40000u;  // divisor's top bit set
    if (un / ud != 1u) return 0;
    if (un % ud != 25535u) return 0;

    un = 65535u;
    ud = 1u;
    if (un / ud != 65535u) return 0;
    if (un % ud != 0u) return 0;

    n = 17;
    d = 5;
    if (n / d != 3) return 0;
    if (n % d != 2) return 0;
    n = -17;
    d = 5;
    if (n / d != -3) return 0;
    if (n % d != -2) return 0;
    n = 17;
    d = -5;
    if (n / d != -3) return 0;
    if (n % d != 2) return 0;
    n = -17;
    d = -5;
    if (n / d != 3) return 0;
    if (n % d != -2) return 0;

    n = 1000;
    n /= 7;
    if (n != 142) return 0;
    n = 1000;
    n %= 7;
    if (n != 6) return 0;

    ln = 1000000000L;
    ld = 7L;
    if (ln / ld != 142857142L) return 0;
    if (ln % ld != 6L) return 0;
    ln = -1000000000L;
    if (ln / ld != -142857142L) return 0;
    if (ln % ld != -6L) return 0;

    return 1;
}
