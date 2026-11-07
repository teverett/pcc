// EXPECT: 1
// Regression test for the same LocalCSE bug as
// test_cse_compound_assign_regression1.c, through a
// CompoundDerefAssign (`*p += n`) instead of a CompoundAssign -
// LocalCSE's own invalidation switch was missing both, so a repeated
// read of the aliased array element could keep reusing a value
// recorded before the pointer write.
int arr[2];
int main() {
    int *p;
    int v;
    arr[0] = 1;
    v = arr[0];      // first "arr[0]" occurrence, recorded
    p = &arr[0];
    *p += 10;         // writes arr[0] through p - must invalidate that recording
    if (v != 1) return 0;
    v = arr[0];      // second "arr[0]" occurrence - must re-read, not reuse the stale 1
    if (v != 11) return 0;
    return 1;
}
