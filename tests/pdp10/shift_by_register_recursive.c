// EXPECT: 1
//
// A runtime-variable shift count is obtained on this target by holding the count in an AC and
// using it as the shift instruction's own index register with a zero displacement (confirmed
// directly against the real simulator - see Pdp10Codegen.h's own header comment); this checks
// that trick still works correctly from inside a recursive call, where the real hardware
// stack (growing *upward* via PUSHJ/PUSH - the opposite direction of every byte-addressable
// target here) has already accumulated several frames' worth of saved frame pointers/return
// addresses/locals below the current one. Each recursion level shifts a running value left by
// its own call depth (a local, so a fresh copy per frame) and right back by the same amount,
// which must round-trip exactly if the per-frame locals and the variable-shift mechanism both
// stay correctly isolated across nested PUSHJ/POPJ frames.
int depthShift(int n, int value) {
    int shifted;
    int back;
    if (n == 0) return value;
    shifted = value << n;
    back = shifted >> n;
    if (back != value) return -1;
    return depthShift(n - 1, value + n);
}

int main() {
    int result;
    result = depthShift(8, 3);
    if (result < 0) return 0;
    return 1;
}
