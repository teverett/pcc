// EXPECT: 55
// A for loop whose condition and post-expression both use ordinary
// operators (i <= n, i = i + 1) accumulates i into sum each pass.
int main() {
    int i;
    int sum;
    sum = 0;
    for (i = 1; i <= 10; i = i + 1) {
        sum = sum + i;
    }
    return sum;  // 1+2+...+10 = 55
}
