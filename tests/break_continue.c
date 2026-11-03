// EXPECT: 23
// Direct break/continue inside a for loop - the real test for
// `continue`: a for-loop's post-expression must still run before the
// condition is retested, even when `continue` skips the rest of the
// body (an old, naive "continue = jump straight back to the loop
// top" implementation would skip the post-expression too and loop
// forever here).
int main() {
    int i;
    int sum;
    sum = 0;
    for (i = 0; i < 10; i = i + 1) {
        if (i == 5) continue;
        if (i == 8) break;
        sum = sum + i;
    }
    return sum;  // 0+1+2+3+4+6+7 = 23 (5 skipped, loop breaks before 8)
}
