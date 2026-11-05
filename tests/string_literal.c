// EXPECT: 5
//
// A string literal ("..." - see grammar/CLexer.g4's own STRINGLIT)
// lowers to the address of a synthetic, anonymous global `char`
// array holding its decoded bytes plus a trailing NUL (see
// IRBuilder.cpp's own buildExpr case) - exactly what `&someCharArray`
// already means for a real declared one. This walks that address
// with ordinary pointer arithmetic to compute the string's own
// length, the same way real C code commonly does.
int strlen_(int p) {
    int i;
    i = 0;
    while (*(p + i) != 0) {
        i = i + 1;
    }
    return i;
}
int main() {
    int p;
    p = "hello";
    return strlen_(p);
}
