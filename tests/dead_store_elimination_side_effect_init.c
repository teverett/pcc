// EXPECT: 7
// Exercises DeadStoreElimination.cpp's documented gap: `x`'s own
// initializer is a side-effecting call (bump(), incrementing `counter`)
// and `x` is never read anywhere - write-only, like `writeOnly` in
// dead_store_elimination.c, but as a LocalDecl's own initializer rather
// than a later plain store. This pass leaves it alone rather than
// stripping it, but bump()'s side effect must still happen exactly once
// either way.
int counter;
int bump() {
    counter = counter + 1;
    return 99;
}
int f() {
    int x = bump();
    return counter;
}
int main() {
    return f() + 6;  // 1 + 6 = 7
}
