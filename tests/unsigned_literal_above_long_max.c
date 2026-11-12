// EXPECT: 1
// `18446744065119617025ULL` exceeds LONG_MAX (2^63-1) - parsing it must wrap into its
// unsigned 64-bit value (as C itself defines) instead of failing to compile.
int main() {
    unsigned long long x;
    x = 18446744065119617025ULL;
    if (x == 18446744065119617025ULL) return 1;
    return 0;
}
