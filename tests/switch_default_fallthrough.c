// EXPECT: 7
// The value matches nothing, so control dispatches straight to
// `default`, which itself falls through (no `break`) into the case
// that follows it - real C allows `default` anywhere, not just last.
int main() {
    int x = 99;
    int result = 0;
    switch (x) {
        case 1:
            result = 1;
            break;
        default:
            result += 3;
        case 2:
            result += 4;
            break;
    }
    return result;
}
