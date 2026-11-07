// EXPECT: 12
// Exercises DeadStoreElimination.cpp's two cases under --optimize: `shadowed`
// is stored twice in a row with no read in between (the first store is
// dead); `sideEffectShadowed`'s first store comes from a call kept only for
// its side effect (bump() incrementing `counter`), with the store itself
// still dropped; `writeOnly` is stored but never read anywhere (dead too).
// `kept`'s own two stores are a case this pass alone wouldn't drop (the
// second one reads the first's value) - ConstantPropagation/ConstantFolding
// fold that read away earlier in the --optimize pipeline, which then lets
// this pass's own shadow case drop the first store too; either way the
// answer must come out the same as with the pass disabled.
int counter;
int bump() {
    counter = counter + 1;
    return 99;
}
int f() {
    int shadowed;
    int sideEffectShadowed;
    int writeOnly;
    int kept;

    shadowed = 1;
    shadowed = 2;

    sideEffectShadowed = bump();  // call kept, store dropped
    sideEffectShadowed = 5;

    writeOnly = 123;

    kept = 3;
    kept = kept + 1;  // reads kept (4), then stores 4

    return shadowed + sideEffectShadowed + kept + counter;  // 2 + 5 + 4 + 1 = 12
}
int main() {
    return f();
}
