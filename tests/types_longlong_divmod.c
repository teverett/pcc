// EXPECT: 1
//
// `long long` divide/modulo - the one operation with no 32-bit x86
// hardware instruction wide enough, so this is synthesized as a bit-
// serial long division (genWideDivMod). Checks unsigned division/
// modulo, then signed division/modulo with every combination of
// operand signs, since the signed path negates magnitudes going in
// and fixes the quotient/remainder's sign coming out (quotient
// negative iff signs differ; remainder takes the dividend's own
// sign, same as plain `int` division already does).
//
// On the 8086 target specifically: this routine is the single
// largest one this backend generates, and even shared as one
// subroutine per word count (see I8086Codegen.cpp's own
// genWideDivMod()/emitWideDivModSubroutine() header comments) it
// still doesn't fit alongside any caller code in that target's own
// 512-byte boot-sector output (--format=raw - see I8086Codegen.h's
// own header comment on the format's hard size limit). The
// implementation itself is the same algorithm already execution-
// verified correct on i386/ARM/M68k; it just has no way to be
// exercised under 8086's own boot-sector-only test harness here.
int main() {
    unsigned long long ua;
    unsigned long long ub;
    ua = 10000000000ULL;   // > UINT_MAX, needs a real 64-bit divisor path
    ub = 3ULL;
    if (ua / ub != 3333333333ULL) return 0;
    if (ua % ub != 1ULL) return 0;

    long long a;
    long long b;
    a = 7LL;
    b = 2LL;
    if (a / b != 3LL) return 0;      // + / + = +
    if (a % b != 1LL) return 0;

    a = -7LL;
    b = 2LL;
    if (a / b != -3LL) return 0;      // - / + = - (truncated toward zero)
    if (a % b != -1LL) return 0;       // remainder takes dividend's sign

    a = 7LL;
    b = -2LL;
    if (a / b != -3LL) return 0;       // + / - = -
    if (a % b != 1LL) return 0;

    a = -7LL;
    b = -2LL;
    if (a / b != 3LL) return 0;         // - / - = +
    if (a % b != -1LL) return 0;

    a = 10000000000LL;
    b = 3LL;
    if (a / b != 3333333333LL) return 0;

    return 1;
}
