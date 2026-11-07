// EXPECT: 99
//
// CompoundDerefAssign (`*p += n`) and Index (`p[0]`) both through a
// plain `int`-as-address pointer variable (see string_literal.c's
// own header comment on this style) that was itself reassigned to an
// address one byte off a string literal's own - ordinary integer
// addition (`p = p + 1`), not real pointer-arithmetic scaling, so
// `p` itself now sits off a native `int`'s own 4-byte boundary for
// every later access through it, including indexing (`p[0]` means
// `*(p + 0)` here, scaled by `p`'s own declared type's width, which
// is `int` either way).
int main() {
    int p;
    p = "abcdef";
    p = p + 1;
    *p = 55;
    *p += 44;
    return p[0];
}
