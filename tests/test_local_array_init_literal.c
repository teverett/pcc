// EXPECT: 24
// A local array's own literal initializer, same arrayInit syntax as
// a global's, just stored on entry to the enclosing function instead
// of at load time.
int main() {
    int a[4] = {2, 4, 6, 12};
    return a[0] + a[1] + a[2] + a[3];  // 2+4+6+12 = 24
}
