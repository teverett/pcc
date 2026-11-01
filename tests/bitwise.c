// EXPECT: 9461
//
// Exercises &, |, ^, ~ on both backends. Each gets a distinct
// decimal "digit" below so a wrong result pins down exactly which
// operator broke.
//
// pdp7 has no hardware `or` at all (see Pdp7Backend.cpp's own
// comment above genBitwise) - `|` is synthesized via De Morgan's
// (~(~a & ~b)), using only `cma` (complement) and the real `and`
// instruction. 8086 has real and/or/xor instructions directly (see
// I8086Backend.cpp).
int main() {
    int a;
    int b;
    a = 12;  // 0b01100
    b = 10;  // 0b01010
    return (a & b) * 1000   // 8
         + (a | b) * 100    // 14
         + (a ^ b) * 10     // 6
         + (~0 == -1);      // 1
}
