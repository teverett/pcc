// EXPECT: 1
//
// Signed division/modulo with every combination of operand signs:
// the quotient always truncates toward zero and the remainder always
// takes the dividend's own sign, regardless of which operand(s) are
// negative.
int main() {
    int a;
    int b;
    a = -17;
    b = 5;
    if (a / b != -3) return 0;
    if (a % b != -2) return 0;
    a = 17;
    b = -5;
    if (a / b != -3) return 0;
    if (a % b != 2) return 0;
    a = -17;
    b = -5;
    if (a / b != 3) return 0;
    if (a % b != -2) return 0;
    a = -20;
    b = 4;
    if (a / b != -5) return 0;
    if (a % b != 0) return 0;
    return 1;
}
