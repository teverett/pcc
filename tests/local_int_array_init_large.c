// EXPECT: 3
// A local int array's initializer keeps values that don't fit in one
// byte intact.
int f() {
    int k[3] = {1000, 256, 65};
    int result;

    result = 0;
    if (k[0] == 1000) {
        result = result + 1;
    }
    if (k[1] == 256) {
        result = result + 1;
    }
    if (k[0] + k[1] + k[2] == 1321) {
        result = result + 1;
    }
    return result;
}

int main() {
    return f();
}
