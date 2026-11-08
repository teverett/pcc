// EXPECT: 3
// A decimal literal too large for int has type long, so it keeps its
// value (and its sign) on a target with a 16-bit int.
long b;

int main() {
    int result;
    result = 0;
    b = 70000;
    if (b == 70000) {
        result = result + 1;
    }
    if (40000 > 0) {
        result = result + 1;
    }
    if (b > 65536) {
        result = result + 1;
    }
    return result;
}
