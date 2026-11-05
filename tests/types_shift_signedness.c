// EXPECT: 1
//
// Right shift divergence: an arithmetic (signed) right shift sign-
// extends from the top bit; a logical (unsigned) right shift always
// fills with 0 instead. The top-bit mask is built from `~0u`'s own
// width (not a hardcoded bit count), so the same check holds
// regardless of which target's own `int` width this runs under (see
// tests/run_tests.sh's own header comment).
int main() {
    int s;
    unsigned int u;
    unsigned int signBit;
    s = -8;
    u = (unsigned int)s;
    signBit = ~(((unsigned int)-1) >> 1);
    if ((s >> 1) != -4) return 0;
    if ((u >> 1) & signBit) return 0;
    return 1;
}
