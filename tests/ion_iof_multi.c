// EXPECT: 21
// Several consecutive _ion()/_iof() calls, interleaved with ordinary
// arithmetic and a function call in between - every target must
// accept any number of these in a row, including one with no real
// interrupt-mask instruction at all (see
// ../src/codegen/i8008/I8008Codegen.cpp's own Intrinsic case, where
// both are plain no-ops), without disturbing anything else.
int bump(int x) {
    return x + 1;
}

int main() {
    int x;
    x = 10;
    _ion();
    _ion();
    _iof();
    x = bump(x);
    _iof();
    _ion();
    x = x + 10;
    return x;
}
