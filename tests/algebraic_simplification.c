// EXPECT: 15
// Exercises every identity AlgebraicSimplification.cpp rewrites - the
// answer must come out the same whether or not `--optimize` is on,
// since this suite doesn't pass that flag (see docs/optimization.md),
// so this is really a base-codegen regression test too.
int f(int a) {
    int x;
    int y;
    int z;
    x = a * 2;
    x = x * 1;
    y = a * -1;
    y = y + 0;
    y = 0 + y;
    z = x | 0;
    z = z ^ 0;
    z = 0 ^ z;
    z = z << 0;
    z = z >> 0;
    z = z - 0;
    return x / 1 + z + y;
}
int main() {
    return f(5);
}
