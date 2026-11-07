// EXPECT: 1
// unsigned char wraps modulo 256 on overflow: incrementing 255 by 1
// wraps back around to 0 rather than becoming 256.
unsigned char c;
int main() {
    c = 255;
    c = c + 1;  // wraps to 0
    if (c != 0) return 0;
    return 1;
}
