// EXPECT: 9
// `break` inside a loop that's nested inside a switch case must exit
// only that loop, not the switch - so the statement after the loop,
// inside the same case, still runs.
int main() {
    int result = 0;
    int i;
    switch (1) {
        case 1:
            for (i = 0; i < 10; i++) {
                if (i == 4) break;
                result += 1;
            }
            result += 5;
            break;
    }
    return result;
}
