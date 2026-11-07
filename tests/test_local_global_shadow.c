// EXPECT: 1
// A local declaration with the same name as a global variable
// shadows it inside that function; anything done to the local
// leaves the global's own storage untouched.
int x = 100;
int f() {
    int x;
    x = 5;
    return x;
}
int main() {
    if (f() != 5) return 0;
    if (x != 100) return 0;
    return 1;
}
