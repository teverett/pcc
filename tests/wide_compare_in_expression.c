// EXPECT: 3
// A long comparison used inside a larger expression: the value
// computed before it is still intact afterward.
long g;

int f() {
    long b;
    int result;

    b = 7;
    result = 0;
    result = result + (b == 7);
    result = result + (b != 8);
    g = 9;
    result = result + (g == 9);
    return result;
}

int main() {
    return f();
}
