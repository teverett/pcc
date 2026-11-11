// EXPECT: 6
//
// WDC 65C02's real rmb0-7/smb0-7 (reset/set bit n of a zero-page byte
// directly, no register involved - M65C02Assembler.cpp) have no C-level
// expression to trigger them, so they're only reachable through the
// `asm { }` escape hatch (see docs/language.md's "Inline asm"). This
// manipulates `ac` itself - the zero-page pseudo-register the compiler's
// own return-value convention already reads back (see
// ../../src/codegen/m65c02/M65C02Codegen.h) - so the result is checkable
// with no other instructions involved: sets bits 0, 1 and 2 (1+2+4=7),
// then clears bit 0 again, leaving bits 1 and 2 (2+4=6).
int main() {
    asm {
        stzp ac
        stzp ac+1
        smb0 ac
        smb2 ac
        smb1 ac
        rmb0 ac
    }
}
