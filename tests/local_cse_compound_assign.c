// EXPECT: 1
// The same comparison expression, textually, appears twice in one
// block with a compound assignment (not a plain `=`) to one of its
// operands in between - local-cse's own subexpression cache has to
// treat that compound assignment as a write to `x`, invalidating any
// earlier `x <op> N` it had cached, the same as a plain `x = ...`
// already does (see ../src/optimizers/local_cse/LocalCSE.cpp's own
// cseExpr() comment: CompoundAssign/CompoundDerefAssign belong in the
// same invalidating group Assign/DerefAssign already are). Without
// that, the second `x != 2` below would wrongly reuse the first one's
// now-stale cached value instead of recomputing it against x's new
// value.
int main() {
    int x;
    x = 2;
    if (x != 2) return 100;  // sanity - must not fire
    x &= 1;   // 0
    x |= 5;   // 5
    x ^= 3;   // 6
    x <<= 2;  // 24
    x >>= 3;  // 3 - genuinely different from the 2 checked above
    if (x != 2) {
        // Correct: x really is 3 now, not the stale cached-false
        // result from the first `x != 2` above.
        return 1;
    }
    return 101;  // only reached if `x != 2` wrongly reused that stale cache
}
