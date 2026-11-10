// EXPECT: 6
// Real C fallthrough: case 1 has no `break`, so control runs straight
// into case 2's body too, accumulating 1 + 2 + 3 = 6.
int main() {
    int x = 1;
    int result = 0;
    switch (x) {
        case 1:
            result += 1;
        case 2:
            result += 2;
        case 3:
            result += 3;
            break;
        case 4:
            result += 100;
    }
    return result;
}
