// EXPECT: 18
// A local array (3+ elements, indices 1 and 2 read, not just 0) that
// a recursive call must get its OWN fresh copy of. Each activation
// fills arr with 4 values derived from its own `n`, recurses, and
// THEN sums arr[0..3] - reading indices 1 and 2 as well as 0 and 3,
// after the nested call has already run and filled (and, under the
// old shared-storage model, clobbered) what would be the same array.
//
// Correct (per-call frame) trace: sumArr(3) fills arr=[3,4,5,6],
// calls sumArr(2) (whose own arr independently goes
// [2,3,4,5]->[1,2,3,4]->[0,1,2,3] as it recurses down to sumArr(0),
// each level's sum discarded since the call is a bare expression
// statement), returns, and sumArr(3)'s own arr is still [3,4,5,6] ->
// 3+4+5+6 = 18.
//
// Under the old one-shared-array-per-declaration model, every level
// writes the SAME array, so by the time sumArr(3) reads it after the
// recursive call returns, it sees the innermost call's last write
// (sumArr(0)'s arr=[0,1,2,3]) instead of its own -> 0+1+2+3 = 6.
int sumArr(int n) {
    int arr[4];
    arr[0] = n;
    arr[1] = n + 1;
    arr[2] = n + 2;
    arr[3] = n + 3;
    if (n > 0) {
        sumArr(n - 1);
    }
    return arr[0] + arr[1] + arr[2] + arr[3];
}

int main() {
    return sumArr(3);
}
