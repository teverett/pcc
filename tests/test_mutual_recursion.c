// EXPECT: 1
// Mutual recursion: each function's own forward declaration (a
// bodyless prototype) lets the other call it before its real
// definition appears textually.
int isEven(int n);
int isOdd(int n);

int isEven(int n) {
    if (n == 0) return 1;
    return isOdd(n - 1);
}
int isOdd(int n) {
    if (n == 0) return 0;
    return isEven(n - 1);
}
int main() {
    if (isEven(10) != 1) return 0;
    if (isOdd(7) != 1) return 0;
    return 1;
}
