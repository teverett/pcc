// EXPECT: 3
// A long long local between two ints leaves both of them intact.
int f() {
    int a;
    long long b;
    int c;
    int result;

    a = 7;
    b = -5;
    c = 9;
    result = 0;
    if (a == 7) {
        result = result + 1;
    }
    if (b == -5) {
        result = result + 1;
    }
    if (c == 9) {
        result = result + 1;
    }
    return result;
}

int main() {
    return f();
}
