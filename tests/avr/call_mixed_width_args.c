// EXPECT: 1
//
// Arguments are pushed right to left and reached through Y (AVRCodegen.h's own header comment);
// a narrower-than-`int` argument (`char`) is still widened to occupy a full native int's worth of
// pushed bytes, and a `long`/`long long` argument occupies its own full width, shifting every
// later parameter's own displacement off Y. Checks a call mixing `char`/`int`/`long`/`long long`
// parameters in one call, a call made while the caller still has an operand pending on the stack,
// `&` of a parameter, and a recursive function taking a `long long`.
long long mix(char a, long long b, int c) { return b * a + c; }
int diff(long long a, long long b) { return (int)(a - b); }
int addrOfParam(int a, int b) {
    int *p;
    p = &b;
    return *p + a;
}
long long sumTo(long long n) {
    if (n == 0LL) return 0LL;
    return n + sumTo(n - 1LL);
}
int one() { return 1; }

int main() {
    int x;
    if (mix(2, 3000000LL, 5) != 6000005LL) return 0;
    if (diff(500000LL, 499990LL) != 10) return 0;
    if (mix(3, mix(2, 10LL, 1), mix(1, 4LL, 5) + 1) != 73LL) return 0;
    x = 100 + mix(2, 7LL, 1) + one() + 1000;  // calls with operands pending on the stack
    if (x != 1116) return 0;
    if (addrOfParam(40, 2) != 42) return 0;
    if (sumTo(100LL) != 5050LL) return 0;
    return 1;
}
