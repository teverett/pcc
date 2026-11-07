// EXPECT: 1
// Regression test for a LocalCSE bug: a repeated `arr[0]` subtree
// must never be hoisted into a temporary when one of its occurrences
// is the direct operand of `&` - `&arr[0]` needs arr[0]'s own real
// storage address, never the address of a separate cached copy.
int arr[3];
int main() {
    int v;
    int *p;
    arr[0] = 10;
    v = arr[0];      // first "arr[0]" occurrence
    p = &arr[0];      // second "arr[0]" occurrence, now under '&'
    *p = 99;          // must write through to the real arr[0]
    if (v != 10) return 0;
    if (arr[0] != 99) return 0;
    return 1;
}
