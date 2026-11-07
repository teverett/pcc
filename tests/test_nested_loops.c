// EXPECT: 36
// Nested for loops: the inner loop's own counter is reset to 1 each
// time the outer loop body runs, so every (i, j) pair from 1..3 is
// visited exactly once.
int main() {
    int i;
    int j;
    int sum;
    sum = 0;
    for (i = 1; i <= 3; i = i + 1) {
        for (j = 1; j <= 3; j = j + 1) {
            sum = sum + i * j;
        }
    }
    return sum;  // (1+2+3)*(1+2+3) = 36
}
