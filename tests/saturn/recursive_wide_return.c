// EXPECT: 1
//
// Exercises this backend's `long` (wide, 2-chunk) return-value protocol layered under real
// recursion: emitEpilogueAndReturn()'s wide path has to pop the return address `gosbvl` pushed
// into a register (`jmpr`) BEFORE pushing the wide result's chunks back on top of it, since a
// plain `rtn` can't be reordered around those extra pushes - get that wrong and a deep call
// chain's return addresses and wide results get interleaved wrong. `sumToLong` recurses n deep,
// each level adding its own `long` contribution and returning a `long`; `fib` is an ordinary int
// recursion (no long) nested inside the same call graph, checking that this backend's software
// stack frames (one per call, built on the `d0`/`d1` registers) don't collide between the two.
long sumToLong(int n) {
    if (n == 0) return 0;
    return (long)n + sumToLong(n - 1);
}

int fib(int n) {
    if (n < 2) return n;
    return fib(n - 1) + fib(n - 2);
}

int main() {
    long s;
    int f;

    s = sumToLong(20);     // 20+19+...+1+0 = 210
    if (s != 210) return 0;

    f = fib(10);            // 55
    if (f != 55) return 0;

    s = sumToLong(5) + (long)fib(6);  // 15 + 8 = 23
    if (s != 23) return 0;

    return 1;
}
