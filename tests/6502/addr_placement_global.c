// EXPECT: 42
//
// Regression test for the now-fixed "'@' addr doesn't pin a global's
// address on the 6502/65c02 backends" bug (see docs/known-bugs.md's history
// and M6502Codegen.cpp/M65C02Codegen.cpp's own emitPlacement()/
// placementLine()): `&pinned` must resolve to the real, assembled address
// the placement directive actually put it at - this backend's own `org`
// base (1536, see M6502Codegen.cpp's kBaseAddress/M65C02Codegen.cpp's own)
// plus the declared offset (8192) - not wherever the ordinary global-
// layout trailer would otherwise place it.
int pinned = 42 @8192;

int main() {
    int addr;
    addr = &pinned;
    if (addr != 1536 + 8192) {
        return -1;
    }
    return pinned;
}
