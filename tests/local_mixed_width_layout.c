// EXPECT: 4
// Locals of different widths declared side by side each keep their
// own storage: writing a wider local never touches a narrower
// neighbour's.
int f() {
    int a;
    long b;
    int c;
    long d;
    int result;

    a = 5;
    b = -3;
    c = 6;
    d = -2;
    result = 0;
    if (a == 5) {
        result = result + 1;
    }
    if (b == -3) {
        result = result + 1;
    }
    if (c == 6) {
        result = result + 1;
    }
    if (d == -2) {
        result = result + 1;
    }
    return result;
}

int main() {
    return f();
}
