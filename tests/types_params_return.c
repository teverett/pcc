// EXPECT: 1
//
// Function parameters and return types of various new widths - the
// calling convention still passes one native-int-or-wider slot per
// argument (see IRBuilder.cpp's own call-argument conversion), but
// each parameter's own frame offset is now computed from its real
// declared type's width, and a `long long` parameter/return spans
// two slots - this exercises the wide-param and wide-return case.
// A narrower-than-int (`char`) parameter is covered on every other
// target but not combined in here: this backend's own boot-sector
// output (--format=raw) has a hard 512-byte budget (see
// I8086Codegen.h's own header comment) that adding it would
// overflow.
long long sumToLL(long long a, long long b) {
    return a + b;
}
int main() {
    if (sumToLL(2000000000LL, 1000000000LL) != 3000000000LL) return 0;
    return 1;
}
