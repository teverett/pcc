// EXPECT: 35
// (expr)() calls through a computed function address rather than a
// literal name - the callee can be a plain variable holding one
// (fp, taken via &f), or a dispatch-table slot (table[i]) read
// straight out of an array. No arguments are supported (see
// IRBuilder.cpp's buildExpr and docs/ir.md's ExprKind::CallPtr note):
// there's no per-callee calling-convention slot to put one in until
// the callee is known, which it never is at compile time here.
int f() { return 42; }
int g() { return 7; }

int table[2];

int callThrough(int p) {
    return (p)();
}

int main() {
    int fp;
    int result;

    fp = &f;
    result = (fp)();  // through a plain variable

    table[0] = &f;
    table[1] = &g;
    result = result + (table[0])();  // through a dispatch-table slot
    result = result - (table[1])();

    result = result - callThrough(&f);  // through a value passed into another function

    return result;  // 42 + 42 - 7 - 42 = 35
}
