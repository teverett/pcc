// EXPECT: 1
// `&f` must resolve to the exact same address a global array's own
// bare-symbol initializer element resolves to.
int f() { return 1; }
int g() { return 2; }
int *cleanF[1] = {f};

int main() {
    int *pf;
    pf = &f;
    if (pf != cleanF[0]) return 0;
    return 1;
}
