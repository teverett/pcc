// EXPECT: 1
//
// genWideDivMod() (Tms320C6713Codegen.cpp) runs its bit-serial restoring division a full 64
// iterations across a register pair, with every intermediate value (n/d/r/q, each 64 bits)
// held in a small stack-relative scratch block rather than registers. This exercises values
// that only resolve correctly if every one of the 64 iterations actually ran (a dividend whose
// only set bit is bit 63) and the unsigned 64-bit "r >= d" compare's own hi-then-lo fallback
// path (equal high words, lower words close together).
int main() {
    unsigned long long a;
    unsigned long long b;

    a = 1ULL << 63;
    b = 1ULL << 62;
    if (a / b != 2ULL) return 0;
    if (a % b != 0ULL) return 0;

    a = (1ULL << 63) + 5ULL;
    b = (1ULL << 63) - 3ULL;
    if (a / b != 1ULL) return 0;
    if (a % b != 8ULL) return 0;

    a = ~0ULL;
    b = ~1ULL;
    if (a / b != 1ULL) return 0;
    if (a % b != 1ULL) return 0;

    long long x;
    long long y;
    x = -9223372036854775807LL - 1LL;  // LLONG_MIN
    y = 2LL;
    if (x / y != -4611686018427387904LL) return 0;
    if (x % y != 0LL) return 0;

    return 1;
}
