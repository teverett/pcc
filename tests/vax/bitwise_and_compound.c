// EXPECT: 1
//
// VAX has no AND instruction: `&` is a complement followed by a bit-clear. Checks it for
// `int`, `unsigned`, `long long` (both words), and compound `&=` through a variable, an
// array element and a pointer, including operands with their sign bit set.
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
    u = 4294967295u;
    if ((u & 65535u) != 65535u) return 0;
    w = -1LL;
    if ((w & 4294967296LL) != 4294967296LL) return 0;  // only the high word survives
    if ((w & 255LL) != 255LL) return 0;                  // only the low word survives
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
