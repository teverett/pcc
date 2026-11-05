// EXPECT: 1
//
// Signed vs. unsigned divergence that a single-type language could
// never express: -1 as a plain (signed) int is less than 0, but the
// same bit pattern reinterpreted as unsigned int is the largest
// representable value, greater than 0. Both outcomes have to be true
// at once for this to return 1 (i.e. both comparisons correct).
int main() {
    int s;
    unsigned int u;
    s = -1;
    u = (unsigned int)s;
    if (s < 0) {
        if (u > 0) {
            return 1;
        }
    }
    return 0;
}
