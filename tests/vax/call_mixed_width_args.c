// EXPECT: 1
//
// Arguments are pushed right to left and reached through `ap` in the callee
// (VaxCodegen.h); a `long long` argument occupies two longwords, shifting every later
// parameter's offset. Also calls made while the caller still has an operand pending on
// the stack, `&` of a parameter, and a recursive function taking a `long long`.
long long mix(int a, long long b, int c) { return b * a + c; }
int diff(long long a, long long b) { return (int)(a - b); }
int addr_of_param(int a, int b) {
    int *p;
    p = &b;
    return *p + a;
}
long long sum_to(long long n) {
    if (n == 0LL) return 0LL;
    return n + sum_to(n - 1LL);
}
int one() { return 1; }

int main() {
    int x;
    if (mix(2, 3000000000LL, 5) != 6000000005LL) return 0;
    if (diff(5000000000LL, 4999999990LL) != 10) return 0;
    if (mix(3, mix(2, 10LL, 1), mix(1, 4LL, 5) + 1) != 73LL) return 0;
    x = 100 + mix(2, 7LL, 1) + one() + 1000;  // calls with operands pending on the stack
    if (x != 1116) return 0;
    if (addr_of_param(40, 2) != 42) return 0;
    if (sum_to(100LL) != 5050LL) return 0;
    return 1;
}
