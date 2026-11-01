// EXPECT: 1064
//
// Exercises << (by a runtime-computed count, not just a literal)
// and >> on both backends.
//
// pdp7 has no hardware variable-count shift at all - the EAE
// `als`/`lrs` instructions bake their count into the instruction
// word at assemble time, not something that can hold a runtime
// value. `<<`/`>>` are implemented instead as a counted loop of
// single-bit doublings/halvings, reusing already-verified `+` and
// the non-negative-only `__divmod` routine respectively (see
// Pdp7Backend.cpp's own comment above genShift). 8086 has real
// shl/shr instructions taking the count from cl directly (see
// I8086Backend.cpp). `>>` is a *logical* shift on both - this
// language has no unsigned type to make the usual signed/logical
// distinction with in the first place.
//
// Kept deliberately small (nothing near the 18-bit/16-bit AC/AX
// boundary - see ../README.md's "Scope and limitations" on int
// width differing per backend): a result anywhere near 2^16 would
// read back as a different (wrapped, negative) value on 8086's
// 16-bit AX than on pdp7's 18-bit AC for the exact same arithmetic,
// which isn't what this test is checking.
int main() {
    int n;
    n = 10;
    return (1 << n) + (640 >> 4);  // 1024 + 40
}
