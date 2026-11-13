// EXPECT: 7
//
// Regression test for the now-fixed "'@' addr doesn't pin a global's or
// function's address on the 6502/65c02 backends" bug (see
// docs/known-bugs.md's history and M6502Codegen.cpp/M65C02Codegen.cpp's own
// emitPlacement()/placementLine()): `&isr` must resolve to the real,
// assembled entry address the placement directive actually put it at -
// this backend's own `org` base (1536) plus the declared offset (8192) -
// and a call through the ordinary symbolic label still has to reach that
// same, correctly-placed body.
int isr() @8192 {
    return 7;
}

int main() {
    int addr;
    addr = &isr;
    if (addr != 1536 + 8192) {
        return -1;
    }
    return isr();
}
