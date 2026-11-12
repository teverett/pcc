// EXPECT: 20
// Same array (`arr`) touched by both a write loop and a read loop - the
// AddressHoisting optimizer pass (src/optimizers/address_hoisting/) should
// compute `arr`'s base address once per function instead of re-deriving it
// at each of these 10 textual accesses; this only checks the result is
// still correct under --optimize, see
// tests/run_pdp7_address_hoisting_tests.sh for a check that the
// hoisting itself actually happened.
int arr[5];
int main() {
    int i;
    int sum;
    for (i = 0; i < 5; i++) {
        arr[i] = i * 2;
    }
    sum = 0;
    for (i = 0; i < 5; i++) {
        sum = sum + arr[i];
    }
    return sum;  // 0+2+4+6+8 = 20
}
