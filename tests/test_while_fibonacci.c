// EXPECT: 55
// Iterative Fibonacci via a while loop: each pass advances (a, b) to
// (b, a+b), so after n passes b holds fib(n+1) and a holds fib(n)
// (with fib(0)=0, fib(1)=1).
int main() {
    int a;
    int b;
    int t;
    int n;
    a = 0;
    b = 1;
    n = 10;
    while (n > 0) {
        t = a + b;
        a = b;
        b = t;
        n = n - 1;
    }
    return a;  // fib(10) = 55
}
