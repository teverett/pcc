// EXPECT: 1
// Mixes a hoistable array write (inside a loop), a read-modify-write
// (`buf[0] = buf[0] + 10`, both a write and a read occurrence), and a
// `&buf[0]` passed to another function - the last of which
// AddressHoisting must NOT fold into its rewrite (an element's own
// address needs codegen's dedicated addressable-operand path, not a
// dereferenced hoisted pointer - see AddressHoisting.cpp's own `Unary
// "&"` exclusion, mirroring LocalCSE.cpp's identical exclusion).
int buf[4];
int sumRange(int *p, int n) {
    int i;
    int total;
    total = 0;
    for (i = 0; i < n; i++) {
        total = total + p[i];
    }
    return total;
}
int main() {
    int i;
    int total;
    for (i = 0; i < 4; i++) {
        buf[i] = i + 1;
    }
    buf[0] = buf[0] + 10;
    total = sumRange(&buf[0], 4);
    return total == 20;  // {11,2,3,4} -> 20
}
