// EXPECT: 58
// Exercises LocalCSE.cpp's own three cases: a redundant array load
// shared with its own first occurrence (`a[i] + a[i]`), a composite
// subexpression shared the same way (`(i + j) * (i + j)`), and an
// `if`'s own body getting its own fresh, unrelated scope (the array
// read inside it is never matched against anything outside it).
int a[3];
int f(int i, int j) {
    int x;
    int y;
    x = a[i] + a[i];
    y = (i + j) * (i + j);
    if (i < j) {
        y = y + a[i];
    }
    return x + y;
}
int main() {
    a[2] = 3;
    return f(2, 5);
}
