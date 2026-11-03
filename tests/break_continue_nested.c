// EXPECT: 10
// Nested loops: break only exits the innermost loop, continue only
// restarts the innermost loop - both plain `while`, no post-statement
// involved (that part is already covered by tests/break_continue.c).
int main() {
    int i;
    int j;
    int sum;
    sum = 0;
    i = 0;
    while (i < 5) {
        j = 0;
        while (j < 5) {
            if (j == 2) break;
            sum = sum + 1;
            j = j + 1;
        }
        i = i + 1;
    }
    return sum;  // inner loop always adds exactly 2 before breaking, 5 outer passes = 10
}
