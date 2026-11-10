// EXPECT: 1
//
// Regression test: AlphaCodegen.cpp's IntLit case loads a non-wide literal via loadImm32(),
// which always sign-extends into the full 64-bit register (the same `ldah`+`lda` sequence any
// signed 32-bit value uses) - correct for `int`, but an `unsigned int` literal whose own bit
// 31 is set (e.g. `2147483648u`) needs the result zero-extended back out instead, or it reads
// back as a negative 64-bit quantity that compares unequal to its own value, less than zero,
// and disagrees with an equal value loaded through a variable instead of a literal.
unsigned int v;

int main() {
    if (2147483648u != 2147483648u) return 0;
    if (!(2147483648u > 0u)) return 0;
    v = 2147483648u;
    if (v != 2147483648u) return 0;
    if (2147483648u != v) return 0;
    if ((2147483648u - 1u) != 2147483647u) return 0;
    if (3000000000u <= 2000000000u) return 0;
    return 1;
}
