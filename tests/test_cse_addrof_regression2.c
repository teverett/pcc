// EXPECT: 1
// Regression test for the same LocalCSE bug as
// test_cse_addrof_regression1.c, in the opposite order: the FIRST
// "arr[0]" occurrence is the one under '&' here, so the risk is
// wrapping IT into `temp = arr[0]` (so the later plain occurrence can
// read the temp instead) - that would turn `&arr[0]` into
// `&(temp = arr[0])`, the address of an unrelated local rather than
// of arr[0] itself.
int arr[3];
int main() {
    int *p;
    int v;
    arr[0] = 5;
    p = &arr[0];      // first "arr[0]" occurrence, under '&'
    v = arr[0];       // second "arr[0]" occurrence, plain read - nothing in
                       // between to otherwise clear a cached first occurrence
    if (v != 5) return 0;
    *p = 9;            // only hits the real arr[0] if p's own address is right
    if (arr[0] != 9) return 0;
    return 1;
}
