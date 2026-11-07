// EXPECT: 1
// A pointer to a pointer: assigning through **pp reaches all the way
// through the intermediate pointer p to the underlying int x.
int x;
int main() {
    int *p;
    int **pp;
    x = 7;
    p = &x;
    pp = &p;
    **pp = 42;
    if (x != 42) return 0;
    if (*p != 42) return 0;
    return 1;
}
