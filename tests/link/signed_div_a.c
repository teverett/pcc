// EXPECT: 4
// Signed division in a separately compiled file runs through the
// linker-supplied runtime: C truncates toward zero, so the quotient
// takes the sign of the operands' product and the remainder the sign
// of the dividend.
extern int quot(int a, int b);
extern int rem(int a, int b);

int main() {
    int result;
    result = 0;
    result = result + (quot(-7, 2) == -3);
    result = result + (rem(-7, 2) == -1);
    result = result + (quot(7, -2) == -3);
    result = result + (rem(7, -2) == 1);
    return result;
}
