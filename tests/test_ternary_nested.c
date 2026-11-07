// EXPECT: 1
// A ternary nested in its own 'else' branch, used purely to classify
// a value into one of three buckets, with no assignment inside
// either branch.
int classify(int x) {
    return x < 0 ? -1 : (x == 0 ? 0 : 1);
}
int main() {
    if (classify(-5) != -1) return 0;
    if (classify(0) != 0) return 0;
    if (classify(5) != 1) return 0;
    return 1;
}
