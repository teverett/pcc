// EXPECT: 1
// Hex literals (0x/0X) parse to the same value as their decimal
// spelling, with the usual u/l suffixes still accepted.
int main() {
    int a = 0x1F;
    unsigned long b = 0XFFUL;
    if (a != 31) return 0;
    if (b != 255) return 0;
    return 1;
}
