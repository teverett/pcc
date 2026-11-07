// EXPECT: 1
// Every relational operator (not just `==`/`!=` - see
// types_sizeof.c/types_sizeof_pointer.c for those), each comparing two
// `sizeof()`s against each other rather than against an assumed
// absolute byte count (a target's own storage unit can be wider than a
// byte - see Type.h's own TypeWidths comment: pdp7 counts in native
// 18-bit words, not bytes, so e.g. sizeof(long long) there is 4, not
// 8). Both operands are IntLit values IRBuilder already knows at
// compile time, so the whole comparison folds away into a plain
// narrow int 0/1 right in IRBuilder.cpp's own ExprKind::Binary case,
// before a `long`'s own `unsigned long` comparison type ever reaches
// a backend that can't actually do wide arithmetic at all (see
// MCS51Codegen.cpp's own requireNarrow()).
int main() {
    if (sizeof(long long) < sizeof(char)) return 0;
    if (sizeof(char) > sizeof(long long)) return 0;
    if (sizeof(int) < sizeof(char)) return 0;
    if (sizeof(long long) <= sizeof(int) && sizeof(int) != sizeof(long long)) return 0;
    if (sizeof(char) >= sizeof(long long) && sizeof(char) != sizeof(long long)) return 0;
    return 1;
}
