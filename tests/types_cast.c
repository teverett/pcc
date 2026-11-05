// EXPECT: 1
//
// Explicit casts: narrowing truncates (and re-sign-extends per the
// target type's own signedness), widening sign/zero-extends per the
// SOURCE's signedness - both directions exercised here.
int main() {
    int big;
    char narrowed;
    unsigned char unarrowed;
    big = 300;               // 0x12C - doesn't fit in a byte
    narrowed = (char)big;     // 300 mod 256 = 44, fits as signed char
    if (narrowed != 44) return 0;

    big = -1;
    unarrowed = (unsigned char)big;  // truncates to 0xFF = 255 unsigned
    if (unarrowed != 255) return 0;

    narrowed = -1;             // signed char -1
    if ((int)narrowed != -1) return 0;      // sign-extend
    if ((unsigned char)narrowed != 255) return 0;  // reinterpret as unsigned byte

    return 1;
}
