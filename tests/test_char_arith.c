// EXPECT: 100
// char operands promote to int before arithmetic, so their sum is
// computed at int width even though each one is stored narrower.
char a;
char b;
char c;
int main() {
    a = 100;
    b = 90;
    c = -90;
    return a + b + c;  // 100 + 90 - 90 = 100, promoted to int throughout
}
