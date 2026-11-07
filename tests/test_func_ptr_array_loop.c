// EXPECT: 60
// A dispatch table of function addresses, called in a loop through
// table[i]() rather than by name.
int fa() { return 10; }
int fb() { return 20; }
int fc() { return 30; }
int table[3];
int main() {
    int i;
    int sum;
    table[0] = &fa;
    table[1] = &fb;
    table[2] = &fc;
    sum = 0;
    for (i = 0; i < 3; i = i + 1) {
        sum = sum + (table[i])();
    }
    return sum;  // 10+20+30 = 60
}
