// EXPECT: 1
// Same gap as local_cse_ternary.c, but for ir::ExprKind::Convert: an
// implicit/explicit conversion repeated in two places must key the same
// way both times (now that exprKey() builds a real key from the source
// type instead of falling back to the node's own address), and the
// converted value must still be correct whether or not LocalCSE hoists
// the shared conversion into a temp. `id()` keeps `c` opaque to constant
// folding/propagation, so the conversion survives into LocalCSE.
char id(char x) {
    return x;
}

int main() {
    char c;
    int x;
    int y;

    c = id(65);
    x = (int)c + 1;  // identical to y's expression below - a prime CSE candidate
    y = (int)c + 1;
    if (x != 66) return 0;
    if (y != 66) return 0;

    c = id(10);
    x = (int)c + 1;  // same shape, different runtime value - must not reuse the cache above
    if (x != 11) return 0;

    return 1;
}
