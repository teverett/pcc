// EXPECT: 60
// The dispatch-table idiom across a real link boundary: an
// extern-declared function's bare name stands for its own address as
// an array-initializer element (see
// ../../docs/pdp-c-compiler.md's "Extern declarations") - both
// fa and fb are only ever defined in extern_dispatch_b.c, resolved
// for real at --link time.
extern int fa();
extern int fb();

int table[2] = {fa, fb};

int main() {
    return (table[0])() + (table[1])();  // 10 + 50 = 60
}
