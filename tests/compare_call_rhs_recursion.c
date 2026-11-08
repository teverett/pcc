// EXPECT: 3
// A comparison whose right operand recursively calls the function
// containing it keeps its left operand per activation: f(n) is n for
// n > 0, since n < f(n - 1) = n - 1 never holds.
int f(int n) {
    if (n == 0) {
        return 0;
    }
    if (n < f(n - 1)) {
        return 50;
    }
    return n;
}

int main() {
    return f(3);
}
