// EXPECT: 184
// Exercises IdenticalCodeFolding.cpp: add1/add2 have identical bodies and
// are safe to merge; fact1/fact2 are identical self-recursive functions
// (each one's own recursive call must still resolve to itself after the
// merge, never cross over to the other); calling through a function
// pointer taken from a folded-away duplicate (ret3b) must still reach the
// merged body.
int add1(int a, int b) {
    return a + b;
}
int add2(int a, int b) {
    return a + b;
}

int fact1(int n) {
    if (n <= 1) return 1;
    return n * fact1(n - 1);
}
int fact2(int n) {
    if (n <= 1) return 1;
    return n * fact2(n - 1);
}

int ret3a() {
    return 3;
}
int ret3b() {
    return 3;
}

int main() {
    int s = add1(3, 4) + add2(10, 20);  // 7 + 30 = 37
    int f = fact1(4) + fact2(5);        // 24 + 120 = 144
    int fp;
    fp = &ret3b;
    int p = (fp)();  // 3, through a pointer to a folded duplicate
    return s + f + p;                   // 37 + 144 + 3 = 184
}
