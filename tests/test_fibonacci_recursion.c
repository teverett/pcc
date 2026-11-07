// EXPECT: 21
// Kept to fib(8) (21 calls' worth of recursion) rather than a bigger
// n: pdp7's own recursion handling is only verified up to this depth/
// call count - see tests/run_tests.sh's own header comment on a .c
// file here meaning the same thing on every target.
int fib(int n) {
    if (n < 2) return n;
    return fib(n - 1) + fib(n - 2);
}
int main() {
    return fib(8);  // 21
}
