// EXPECT: 1
// A pointer argument to an element of a global array lets the callee
// write through it straight into the caller's own array storage,
// not a copy.
int arr[4];
int zeroFirst(int *p) {
    *p = 0;
    return 0;
}
int main() {
    arr[0] = 99;
    arr[1] = 1;
    arr[2] = 2;
    arr[3] = 3;
    zeroFirst(&arr[0]);
    if (arr[0] != 0) return 0;
    return 1;
}
