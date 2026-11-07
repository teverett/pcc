// EXPECT: 1
// Same idea as local_cse_compound_assign.c, but the write in between
// is a compound assignment through a pointer (`*p += ...`) rather
// than a plain variable - local-cse's own cseExpr() has to treat
// CompoundDerefAssign exactly as conservatively as DerefAssign
// already is (clearing every cached subexpression, since the target
// address is a runtime value it can't reason about), or a repeated
// `y != 0` after `*p += 5` would wrongly reuse a cached result from
// before the write.
int main() {
    int y;
    int *p;
    y = 0;
    p = &y;
    if (y != 0) return 100;  // sanity - must not fire
    *p += 5;  // y is now 5, not 0
    if (y != 0) {
        return 1;  // correct: y really changed
    }
    return 101;  // only reached if the second `y != 0` wrongly reused stale cache
}
