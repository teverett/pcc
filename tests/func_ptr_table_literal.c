// EXPECT: 35
// A literal array initializer can name functions directly - the real
// s1.s dispatch-table idiom (".sysloc"/"locsw"/"locn"-style jump
// tables): each element is that function's own address, resolved by
// IRBuilder.cpp the same way a bare "&name" expression is (see IR.h's
// ArrayInitElem). Calling through a slot still goes through the
// (expr)() syntax (ExprKind::CallPtr) - this only covers how the
// table itself gets built.
int f() { return 42; }
int g() { return 7; }

int table[2] = { f, g };

int main() {
    return (table[0])() - (table[1])();
}
