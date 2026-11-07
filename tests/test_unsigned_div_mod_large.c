// EXPECT: 1
//
// Unsigned division/modulo of values whose top bit is set must use the
// full magnitude, never a signed (sign-extended) interpretation: 60000
// and 50000 are both large positive values as `unsigned int`, even
// though their top bit would make them negative as a plain `int`.
// The divisors are kept close to the dividends so even a slow
// repeated-subtraction division core only loops a handful of times.
int main() {
    unsigned int u;
    u = 60000;
    if (u / 19999 != 3) return 0;
    if (u % 19999 != 3) return 0;
    u = 50000;
    if (u / 12500 != 4) return 0;
    if (u % 12500 != 0) return 0;
    return 1;
}
