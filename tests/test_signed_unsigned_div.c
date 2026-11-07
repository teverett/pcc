// EXPECT: 1
// Signed division truncates toward zero (-7/2 == -3, not -4);
// unsigned division of the corresponding positive magnitude is
// checked separately so the two never depend on one target's own
// int width to line up.
int main() {
    int s;
    unsigned int u;
    s = -7;
    if (s / 2 != -3) return 0;
    u = 7;
    if (u / 2 != 3) return 0;
    return 1;
}
