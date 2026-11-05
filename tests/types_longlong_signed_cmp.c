// EXPECT: 1
//
// Signed vs. unsigned `long long` comparison divergence - the same
// idea as types_signed_unsigned.c, one dword wider: a negative long
// long's bit pattern, reinterpreted as unsigned long long, is
// enormous (greater than 1) rather than negative (less than 1).
// Kept to a single check: this backend's own boot-sector output
// (--format=raw) has a hard 512-byte budget (see I8086Codegen.h's
// own header comment) that a second wide statement here would
// overflow.
int main() {
    if (!((unsigned long long)-1LL > 1ULL)) return 0;
    return 1;
}
