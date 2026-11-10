// EXPECT: 0
// A nested `if` body gets its own subexpression cache scope in
// local-cse (see ../src/optimizers/local_cse/LocalCSE.cpp's cseBlock),
// but a write inside that nested body still has to invalidate any
// matching candidate the enclosing scope cached before the nested body
// ran. x and y are parameters, so neither constant-folding nor
// constant-propagation can resolve `x + y` ahead of time - only
// local-cse's own cache can wrongly reuse it. Without cross-scope
// invalidation, the second `x + y == a` below would wrongly reuse the
// first `x + y`'s now-stale cached value instead of recomputing it
// against y's new value.
int f(int x, int y) {
    int a;
    a = x + y;
    if (x > 0) {
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
