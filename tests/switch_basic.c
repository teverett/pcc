// EXPECT: 20
// Each case ends in `break`, so this behaves like a plain dispatch:
// multiple case labels (2 and 3) sharing one body, and a `default` that
// only fires when nothing else matches.
int main() {
    int x = 2;
    int result = 0;
    switch (x) {
        case 1:
            result = 10;
            break;
        case 2:
        case 3:
            result = 20;
            break;
        default:
            result = 99;
    }
    return result;
}
