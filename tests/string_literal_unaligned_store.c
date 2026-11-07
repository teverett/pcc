// EXPECT: 42
//
// Stores through, then reads back, a plain `int`-as-address pointer
// (see string_literal.c's own header comment on this style) one byte
// past a string literal's own address - DerefAssign and Unary "*"
// (see src/ast/AST.h's own ExprKind comments) each transfer a full
// native `int`, so this address is one byte off that width's own
// natural 4-byte boundary, exactly like string_literal.c's own `p +
// i` for an odd `i`, just through a store instead of only a load.
int main() {
    int p;
    p = "abcdef";
    *(p + 1) = 42;
    return *(p + 1);
}
