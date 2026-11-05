// EXPECT: 30
//
// Exercises integer promotion and the usual arithmetic conversions:
// char/short operands always promote to int before arithmetic, and a
// char/short local's own storage is narrower than int (so this also
// exercises narrower-than-native-int loads/stores on every backend
// wide enough to have one).
char a;
short b;
unsigned char c;
int main() {
    a = 10;
    b = 15;
    c = 5;
    return a + b + c;  // 10 + 15 + 5 = 30, all promoted to int
}
