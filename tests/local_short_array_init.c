// EXPECT: 3
// A local short array's initializer stores each element at its own
// width, and the code after it still runs normally.
int f() {
    short h[3] = {300, 2, 7};
    short z[2];
    int result;

    result = 0;
    if (h[0] + h[1] + h[2] == 309) {
        result = result + 1;
    }
    if (h[1] == 2) {
        result = result + 1;
    }
    z[0] = 3;
    z[1] = 4;
    if (z[0] == 3 && z[1] == 4 && h[2] == 7) {
        result = result + 1;
    }
    return result;
}

int main() {
    return f();
}
