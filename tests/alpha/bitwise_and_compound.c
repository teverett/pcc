// EXPECT: 1
//
// Alpha's `and`/`bis`/`xor` are ordinary 64-bit operate-format instructions (AlphaAssembler.h),
// so unlike VAX this target needs no bit-clear/complement trick - but every result still goes
// through AlphaCodegen.cpp's narrowT0To() afterward, since the register holding each operand
// is 64 bits regardless of the C type's own width. Checks `&` for `int`, `unsigned`, and
// `long long` (a single 64-bit register, not a word pair), including operands with their sign
// bit set, plus compound `&=` through a plain variable, an array element and a pointer.
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
    if ((w & 4294967296LL) != 4294967296LL) return 0;
    if ((w & 255LL) != 255LL) return 0;
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

    w = -1LL;
    w = w | 0LL;
    if (w != -1LL) return 0;
    w = w ^ (-1LL);
    if (w != 0LL) return 0;
    return 1;
}
