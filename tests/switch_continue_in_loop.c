// EXPECT: 2
// `continue` reached from directly inside a switch body (not inside a
// further-nested loop) must still target the *enclosing loop*, not the
// switch - so odd iterations skip straight to the for-loop's post
// expression, never reaching the `total += 100` below the switch.
int main() {
    int total = 0;
    int i;
    for (i = 0; i < 4; i++) {
        switch (i % 2) {
            case 1:
                continue;
        }
        total += i;
    }
    return total;
}
