// EXPECT: 123
// Isolated check for a Local array's own per-element frame address
// (see each migrated backend's own addr()/genIndexAddr() comments):
// index 1, 2 and 3 are all read, not just index 0, and the per-
// element weights below are chosen so indexing in the wrong
// direction, or mixing up which element is which, changes the
// answer rather than merely its magnitude.
int main() {
    int a[4];
    a[0] = 1000;
    a[1] = 100;
    a[2] = 10;
    a[3] = 1;
    return a[1] * 1 + a[2] * 2 + a[3] * 3;
}
