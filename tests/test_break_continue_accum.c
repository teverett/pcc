// EXPECT: 1
// `continue` skips straight to the for loop's own post-expression
// (not back to the top of the body), and `break` exits the loop
// entirely rather than just the current iteration.
int main() {
    int i;
    int sum;
    sum = 0;
    for (i = 1; i <= 20; i = i + 1) {
        if (i % 3 == 0) continue;   // skip multiples of 3
        if (i > 10) break;          // stop once past 10
        sum = sum + i;
    }
    if (sum != 37) return 0;  // 1+2+4+5+7+8+10 = 37
    return 1;
}
