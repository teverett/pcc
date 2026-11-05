// EXPECT: 1
//
// `long long` add whose result exceeds a native 32-bit int's range -
// proves the ripple-carry add (genWideAdd) actually carries across
// the dword boundary, not just coincidentally produces the right low
// 32 bits. Kept to one check: this backend's own boot-sector output
// (--format=raw) has a hard 512-byte budget (see I8086Codegen.h's
// own header comment) that a second wide statement here would
// overflow.
int main() {
    long long a;
    a = 5000000000LL + 3000000000LL;  // 8000000000 - overflows 32 bits even unsigned
    if (a != 8000000000LL) return 0;
    return 1;
}
