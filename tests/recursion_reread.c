// EXPECT: 5
//
// A discriminating recursion test: `n` is re-read AFTER the nested
// recursive call returns, not just once before it. A backend whose
// Local/Param storage is still one shared, permanent cell per
// declaration (rather than a fresh per-call frame) gets this wrong -
// every nested call overwrites the same shared `n` cell, so by the
// time the outermost call's own "return n" runs, it reads whatever
// the deepest call last left behind (0), not its own argument (5).
// Only a real per-call frame (see e.g.
// src/codegen/m65c02/M65C02Codegen.h's own frame-pointer comment)
// keeps each activation's own `n` independent of every other.
int f(int n) {
    if (n <= 0) {
        return 0;
    }
    f(n - 1);
    return n;
}

int main() { return f(5); }
