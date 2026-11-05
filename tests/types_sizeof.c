// EXPECT: 1
//
// sizeof(T) checked against relationships the C standard actually
// guarantees (char is 1 byte, width only grows going char -> long
// long, and sizeof(expr) matches the expression's own static type)
// rather than any one target's own concrete byte counts, so this
// holds the same way on every backend (see tests/run_tests.sh's own
// header comment: a .c file here means the same thing on every
// target). Kept short: every sizeof() comparison here is a wide
// (`unsigned long`) one, and this backend's own boot-sector output
// (--format=raw) has a hard 512-byte budget - too many of them in
// one function would overflow it (see I8086Codegen.h's own header
// comment).
int x;
int main() {
    if (sizeof(char) != 1) return 0;
    if (sizeof(char) > sizeof(long long)) return 0;
    if (sizeof(x) != sizeof(int)) return 0;
    return 1;
}
