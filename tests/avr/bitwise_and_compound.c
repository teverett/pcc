// EXPECT: 1
//
// AVR has a real `and`/`or`/`eor` at every width, applied byte-at-a-time with no carry
// (AVRCodegen.cpp's combineBytes()); checks `&` for `int`, `unsigned`, and `long long` (every
// byte), including operands with their sign bit set, plus compound `&=` through a variable, an
// array element and a pointer.
int a;
int arr[2];
unsigned int u;
long long w;

int main() {
    int *p;
    if ((12 & 10) != 8) return 0;
    if ((-1 & 255) != 255) return 0;
    if ((-8 & 15) != 8) return 0;
    if ((-1 & -256) != -256) return 0;
    u = 65535u;
    if ((u & 255u) != 255u) return 0;
    w = -1LL;
    if ((w & 4294967296LL) != 4294967296LL) return 0;  // only a high byte survives
    if ((w & 255LL) != 255LL) return 0;                  // only the low byte survives
    w = 1311768467463790320LL;
    if ((w & 4294967295LL) != 2596069104LL) return 0;

    a = 255;
    a &= 6;
    if (a != 6) return 0;
    arr[1] = -1;
    arr[1] &= 1000;
    if (arr[1] != 1000) return 0;
    p = &a;
    *p &= 4;
    if (a != 4) return 0;
    return 1;
}
