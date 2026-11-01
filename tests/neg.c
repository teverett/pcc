// EXPECT: 107
int main() {
    int x;
    x = -5;
    x = x + 12;      // 7
    if (-x < 0) {    // -7 < 0, true
        x = x + 100; // 107
    }
    return x;
}
