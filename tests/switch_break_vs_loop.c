// EXPECT: 13
// `break` inside a switch that's nested in a loop must exit only the
// switch, not the loop (real C semantics) - so the loop keeps running
// for every iteration, each adding to `total` via the switch.
int main() {
    int total = 0;
    int i;
    for (i = 0; i < 4; i++) {
        switch (i) {
            case 0:
                total += 1;
                break;
            case 1:
                total += 2;
                break;
            default:
                total += 5;
                break;
        }
    }
    return total;
}
