// EXPECT: 1
// sizeof() of a pointer type/expression, compared the same way
// types_sizeof.c already checks sizeof(char)/sizeof(long long): every
// pointer has the same size regardless of its pointee, and sizeof(p)
// matches sizeof(that same pointer type) spelled out explicitly. Each
// comparison here is between two values already known at compile
// time (see IRBuilder.cpp's own ExprKind::Binary case: a `sizeof` is
// already an IntLit, of type `unsigned long`, before this comparison
// is even built) - folded away into a plain narrow int 0/1 right
// there, so it never needs any actual wide comparison *instruction*
// to decide, not even on a target with no wide-arithmetic support of
// its own at all (see MCS51Codegen.cpp's own requireNarrow()).
int main() {
    int *p;
    char *cp;
    if (sizeof(p) != sizeof(int *)) return 0;
    if (sizeof(cp) != sizeof(char *)) return 0;
    if (sizeof(int *) != sizeof(char *)) return 0;
    return 1;
}
