// EXPECT: 1
// Several `long long` divisions and moduli in the same function,
// signed and unsigned alike - exercises genWideDivMod() being shared
// as a single real subroutine per (signedness, word count) pair,
// `cal`-ed from every one of these instead of duplicated in full at
// each one (see ../src/codegen/i8008/I8008Codegen.cpp's own
// getWideDivModSubroutine() comment) - a backend that still inlined
// every one of these separately would multiply its own already
// sizable body by however many of them a program happens to have.
int main() {
    long long a;
    long long b;
    unsigned long long ua;
    unsigned long long ub;

    a = 20LL;
    b = 3LL;
    if (a / b != 6LL) return 100;
    if (a % b != 2LL) return 101;

    a = -20LL;
    b = 3LL;
    if (a / b != -6LL) return 102;
    if (a % b != -2LL) return 103;

    ua = 1000000ULL;
    ub = 7ULL;
    if (ua / ub != 142857ULL) return 106;
    if (ua % ub != 1ULL) return 107;

    return 1;
}
