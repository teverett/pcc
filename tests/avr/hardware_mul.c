// EXPECT: 1
//
// AVR has a real unsigned 8x8->16 `mul` (forced into r1:r0); AVRCodegen.cpp's genNarrowMul()
// builds a 16x16->16 product from three of them (lo*lo, lo*hi, hi*lo), keeping only the low 16
// bits of the true product the way C's `*` silently wraps. Checks operands whose low byte alone
// already carries into the high byte, operands with no low-byte contribution at all, a negative
// (two's complement) operand, and plain small values.
unsigned int ua;
unsigned int ub;
int a;
int b;

int main() {
    ua = 200u;
    ub = 200u;
    if (ua * ub != 40000u) return 0;  // fits in 16 bits unsigned, no wrap

    ua = 300u;
    ub = 300u;
    if (ua * ub != 24464u) return 0;  // 90000 mod 65536

    ua = 256u;
    ub = 256u;
    if (ua * ub != 0u) return 0;  // hi*hi term entirely discarded by a 16-bit result

    a = -5;
    b = 7;
    if (a * b != -35) return 0;

    a = -5;
    b = -7;
    if (a * b != 35) return 0;

    ua = 1000u;
    ub = 1000u;
    if (ua * ub != 16960u) return 0;  // 1000000 mod 65536

    a = 0;
    b = 12345;
    if (a * b != 0) return 0;

    a = 1;
    b = -1;
    if (a * b != -1) return 0;

    return 1;
}
