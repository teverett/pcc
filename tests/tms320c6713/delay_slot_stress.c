// EXPECT: 1
//
// Stresses this backend's own mandatory delay-slot NOP padding (branch: 5, load: 4, multiply:
// 1 - see Tms320C6713Codegen.h's own header comment) back-to-back inside a tight loop: every
// iteration loads two array elements, multiplies them, accumulates, and branches - if any one
// of those NOPs were ever dropped, the simulator's own faithfully-timed pipeline model (see
// run_tests.sh's own check_tms320c6713()) would read a stale operand and this would no longer
// come out to the closed-form answer below.
int main() {
    int data[10];
    int i;
    long sum;
    int product;

    i = 0;
    while (i < 10) {
        data[i] = i + 1;
        i = i + 1;
    }

    sum = 0;
    i = 0;
    while (i < 10) {
        product = data[i] * data[9 - i];
        sum = sum + product;
        i = i + 1;
    }
    // sum of (i+1)*(10-i) for i=0..9 = 220
    if (sum != 220) return 0;

    // Back-to-back loads feeding an immediately-following multiply, no scheduling at all.
    int a;
    int b;
    int total;
    total = 0;
    i = 0;
    while (i < 9) {
        a = data[i];
        b = data[i + 1];
        total = total + a * b;
        i = i + 1;
    }
    // sum_{i=0..8} (i+1)*(i+2) = 2+6+12+20+30+42+56+72+90 = 330
    if (total != 330) return 0;

    return 1;
}
