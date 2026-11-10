// EXPECT: 1
// LocalCSE's exprKey() didn't have an explicit case for
// ir::ExprKind::Ternary, so a repeated `cond ? then : else` subexpression
// always keyed off its own node address and could never be recognized as
// a match for an identical later occurrence - a missed optimization, not
// a wrong-result bug, but worth covering now that Ternary gets a real
// structural key: two structurally identical ternaries must still
// evaluate to the same (correct) value whether or not CSE hoists them.
// `id()` keeps `a`/`b` opaque to constant folding/propagation, so the
// ternary survives into LocalCSE instead of being collapsed earlier.
int id(int x) {
    return x;
}

int main() {
    int a;
    int b;
    int r1;
    int r2;

    a = id(5);
    b = id(3);
    r1 = (a > b ? a : b) + 1;  // (5>3?5:3)+1 = 6
    r2 = (a > b ? a : b) + 1;  // identical to r1 - a prime CSE candidate
    if (r1 != 6) return 0;
    if (r2 != 6) return 0;

    // Same values, condition flipped: the ternary nested inside this
    // repeated `(...) + 1` must still key differently from r1/r2's,
    // or this would wrongly reuse their cached `6`.
    r1 = (a < b ? a : b) + 1;  // (5<3?5:3)+1 = 4
    if (r1 != 4) return 0;

    return 1;
}
