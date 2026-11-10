// EXPECT: 1
//
// This target's `int*int` has no native 32x32 multiply at all (genMulLow32() in
// Tms320C6713Codegen.cpp) - the standard TI MPY/MPYHL/MPYLH decomposition sign-extends each
// operand's own low halfword before multiplying, which is only "almost" low32(A*B): whenever
// an operand's low 16 bits have their own bit 15 set (so MPY treats that halfword as negative
// even though the full 32-bit value may not be), the plain three-term sum comes out short by
// exactly 0x10000 times the other operand - genMulLow32() corrects for this explicitly. This
// exercises exactly that edge: every operand below has a low halfword with bit 15 set.
int main() {
    int a;
    int b;
    a = 5;
    b = -1;
    if (a * b != -5) return 0;

    a = -1;
    b = -1;
    if (a * b != 1) return 0;

    unsigned int ua;
    unsigned int ub;
    ua = 4294901760u;  // 0xFFFF0000: low halfword is exactly 0, bit 15 clear, high half's own bit set
    ub = 2u;
    if (ua * ub != 4294836224u) return 0;  // 0xFFFE0000 (wraps mod 2^32)

    ua = 305430528u;  // 0x12348000: low halfword 0x8000 - bit 15 set
    ub = 3u;
    if (ua * ub != 916291584u) return 0;  // 0x12348000 * 3, no overflow past 32 bits

    a = 40000;
    b = -40000;
    if (a * b != (40000 * -40000)) return 0;

    return 1;
}
