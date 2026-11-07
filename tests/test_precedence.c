// EXPECT: 1
// Operator precedence matches ordinary C: `*`/`/`/`%` bind tighter
// than `+`/`-`, and parentheses override that default grouping.
int main() {
    int r;
    r = 2 + 3 * 4;        // 2 + 12 = 14
    if (r != 14) return 0;
    r = 20 - 4 / 2;        // 20 - 2 = 18
    if (r != 18) return 0;
    r = 10 % 3 + 2 * 5;    // 1 + 10 = 11
    if (r != 11) return 0;
    r = (2 + 3) * 4;       // 20
    if (r != 20) return 0;
    return 1;
}
