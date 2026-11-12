// EXPECT: 7
// Same optimization as compare_constant_induction_loop.c, but against a
// negative literal and with an unknown-sign variable that starts
// negative and stays negative on every iteration - exercises both the
// `signB == 1` dispatch branch and the precomputed-negation fast path in
// Pdp7Codegen.cpp's emitSignedLtCells() together.
int main() {
    int i;
    int count;
    count = 0;
    for (i = -10; i < -3; i++) {
        count = count + 1;
    }
    return count;  // i = -10..-4, 7 iterations
}
