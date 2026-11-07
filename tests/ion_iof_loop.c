// EXPECT: 10
// _ion()/_iof() called repeatedly from inside a loop - every target
// must accept this (even one with no real interrupt-mask instruction
// at all, where the two are plain no-ops - see
// ../src/codegen/i8008/I8008Codegen.cpp's own Intrinsic case), and
// the surrounding loop arithmetic must be unaffected by either call.
int main() {
    int i;
    int total;
    i = 0;
    total = 0;
    while (i < 5) {
        _iof();
        total = total + 1;
        _ion();
        total = total + 1;
        i = i + 1;
    }
    return total;
}
