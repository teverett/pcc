// EXPECT: 1
// &f (a plain function name) is that function's own entry address -
// see IR.h's ExprKind::Func and, on pdp7, Pdp7Codegen::genAddrOf,
// which emits "law FN_<name>", the same entry label a direct call
// already targets. Checked here via comparison only (no indirect
// call through the result yet): the same function taken twice gives
// the same address, and two different functions give different ones.
int f() { return 1; }
int g() { return 2; }

int main() {
    int pf;
    int pf2;
    int pg;
    int ok;

    pf = &f;
    pf2 = &f;
    pg = &g;

    ok = 0;
    if (pf == pf2) {
        if (pf != pg) {
            ok = 1;
        }
    }
    return ok;
}
