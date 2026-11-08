// EXPECT: 4
// Unsigned division at the edges: a divisor of 1 returns the dividend
// whole, a dividend smaller than the divisor gives quotient 0 and
// returns itself as the remainder, and both hold for values with the
// top bit set.
unsigned int a;
unsigned int b;
unsigned int one;

int main() {
    int result;

    result = 0;
    a = 0u - 6;
    one = 1;
    result = result + (a / one == a);
    result = result + (a % one == 0);
    b = 0u - 5;
    result = result + (a / b == 0);
    result = result + (a % b == a);
    return result;
}
