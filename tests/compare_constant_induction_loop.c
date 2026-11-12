// EXPECT: 45
// The classic for-loop induction pattern (`i < 10` against a non-negative
// literal, `i` itself of unknown sign to the codegen) that
// Pdp7Codegen.cpp's genCompareCells()/emitSignedLtCells() now lower via a
// precomputed negated-literal cell instead of negating the literal at
// runtime on every iteration - see tests/run_pdp7_compare_constant_tests.sh
// for a check that the lowering itself actually changed; this only checks
// the result is still correct, here and on every other target.
int main() {
    int i;
    int sum;
    sum = 0;
    for (i = 0; i < 10; i++) {
        sum = sum + i;
    }
    return sum;  // 0+1+...+9 = 45
}
