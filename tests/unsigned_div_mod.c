// EXPECT: 4
// Unsigned `/` and `%` treat operands with the top bit set as large
// values, not negatives: (UMAX-5) / (UMAX-6) is 1 remainder 1 (signed
// division would give 0 remainder -6), and (UMAX-1) / 2 is a quotient
// q with q+q == UMAX-1, remainder 0 (signed would give q == UMAX).
// The operands are globals so the division happens at run time.
unsigned int a;
unsigned int b;
unsigned int two;

int main() {
    unsigned int q;
    unsigned int r;
    int result;

    a = 0u - 6;
    b = 0u - 7;
    two = 2;
    result = 0;
    result = result + (a / b == 1);
    result = result + (a % b == 1);
    q = (a + 4) / two;
    r = (a + 4) % two;
    result = result + (r == 0 && q + q == a + 4);
    result = result + (q != 0u - 1);
    return result;
}
