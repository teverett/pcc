// EXPECT: 0
// Same scoping hazard as local_cse_nested_if_invalidation.c, but for a
// `while` body: x and y are parameters, so `x + y` computed once
// before the loop can't be resolved by constant-folding or
// constant-propagation - only local-cse's own cache can wrongly reuse
// it. The loop body (its own nested cache scope) writes y on every
// iteration, so the second `x + y == a` after the loop must not reuse
// the pre-loop cached value; a cache that only clears its own scope on
// a write, and never the scopes it is nested inside, would wrongly
// hand that stale value back.
int f(int x, int y) {
    int a;
    a = x + y;
    while (y < 5) {
        y = y + 1;
    }
    if (x + y == a) {
        return 1;  // wrong: would fire if the stale cached `x + y` were reused
    }
    return 0;
}

int main() {
    return f(1, 2);
}
