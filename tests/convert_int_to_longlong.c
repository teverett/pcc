// EXPECT: 3
// Widening an int to long long sign-extends a negative value and
// zero-extends a non-negative one.
int f() {
    int i;
    long long v;
    long long w;
    int result;

    i = -7;
    v = i;
    i = 12;
    w = i;
    result = 0;
    if (v == -7) {
        result = result + 1;
    }
    if (v < 0) {
        result = result + 1;
    }
    if (w == 12) {
        result = result + 1;
    }
    return result;
}

int main() {
    return f();
}
