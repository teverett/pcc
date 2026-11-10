// EXPECT: 1
//
// PDP-10's native word is 36 bits, not 32 (Pdp10Codegen.h's own `typeWidths()`), so a plain
// `int` here holds values well beyond what a 32-bit-int target could represent without
// overflow - 100000*100000 (10,000,000,000) doesn't fit in a 32-bit `int` (max ~2.1 billion),
// but fits comfortably in this target's 36-bit one (max ~34.3 billion). This also exercises
// `MUL`'s own real placement of the truncated single-length product: confirmed directly
// against the simulator that it lands in `AC+1`, not `AC` - genMul() copies it back into
// AC1 afterward, and this checks the full 35-bit-magnitude result survives that round trip.
// Also checks IDIV recovers the same large value's factors back out exactly.
int main() {
    int a;
    int b;
    int product;
    a = 100000;
    b = 100000;
    product = a * b;
    if (product != 10000000000) return 0;
    if (product / a != b) return 0;
    if (product % b != 0) return 0;
    if ((product + 1) % b != 1) return 0;
    return 1;
}
