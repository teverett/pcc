// EXPECT: 1
// Several independent early-return branches in one function, each
// reached through a different condition.
int classify(int x) {
    if (x < -100) return 1;
    if (x < 0) return 2;
    if (x == 0) return 3;
    if (x < 100) return 4;
    return 5;
}
int main() {
    if (classify(-200) != 1) return 0;
    if (classify(-5) != 2) return 0;
    if (classify(0) != 3) return 0;
    if (classify(50) != 4) return 0;
    if (classify(500) != 5) return 0;
    return 1;
}
