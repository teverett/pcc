// EXPECT: 3
// A local array's initializer accepts negated literals, including in
// a narrower element type.
int f() {
    int a[3] = {-1, 2, -7};
    char c[2] = {-5, 9};
    int result;

    result = 0;
    if (a[0] == -1 && a[2] == -7) {
        result = result + 1;
    }
    if (a[0] + a[1] + a[2] == -6) {
        result = result + 1;
    }
    if (c[0] == -5 && c[1] == 9) {
        result = result + 1;
    }
    return result;
}

int main() {
    return f();
}
