// EXPECT: 1
// A function with many separate early-return statements, each
// reached only after several prior checks already passed - stresses
// a backend whose function epilogue (frame teardown) is shared across
// every `return` in the same function (see
// ../src/codegen/i8008/I8008Codegen.cpp's own `currentEpilogueLabel_`
// comment) rather than inlined fresh at each one: every one of these
// has to land in the same place and tear the frame down correctly, in
// particular the final implicit fall-through at the very end, which
// never executes an explicit `return` jump at all.
int main() {
    int a;
    int b;
    int c;
    int d;
    a = 1;
    b = 2;
    c = 3;
    d = 4;
    if (a != 1) return 100;
    if (b != 2) return 101;
    if (c != 3) return 102;
    if (d != 4) return 103;
    if (a + b != 3) return 104;
    if (b + c != 5) return 105;
    if (c + d != 7) return 106;
    if (a + b + c + d != 10) return 107;
    if (a * b != 2) return 108;
    if (b * c != 6) return 109;
    if (c * d != 12) return 110;
    if (d - a != 3) return 111;
    if (d / b != 2) return 112;
    if (d % c != 1) return 113;
    return 1;
}
