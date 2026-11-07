// EXPECT: 1
// `&&` and `||` short-circuit: the right operand is only ever
// evaluated when the left one alone doesn't already decide the
// result, observed here through a side effect in the right operand.
int sideEffects;
int touch() {
    sideEffects = sideEffects + 1;
    return 1;
}
int main() {
    int r;
    sideEffects = 0;
    r = 0 && touch();   // left is false: touch() never runs
    if (sideEffects != 0) return 0;
    r = 1 || touch();   // left is true: touch() never runs
    if (sideEffects != 0) return 0;
    r = 1 && touch();   // left is true: touch() DOES run
    if (sideEffects != 1) return 0;
    r = 0 || touch();   // left is false: touch() DOES run
    if (sideEffects != 2) return 0;
    if (r != 1) return 0;
    return 1;
}
