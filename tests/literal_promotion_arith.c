// EXPECT: 2
// Arithmetic on a literal too large for int is done at long width.
long r;

int main() {
    int result;
    result = 0;
    r = 100000 + 1;
    if (r - 100000 == 1) {
        result = result + 1;
    }
    r = 300000;
    if (r == 150000 + 150000) {
        result = result + 1;
    }
    return result;
}
