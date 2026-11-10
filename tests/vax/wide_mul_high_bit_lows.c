// EXPECT: 1
//
// `long long` multiply (VaxCodegen.cpp's genWideMul) builds the 64-bit product from
// `emul`, which multiplies the low words as *signed* longwords. A low word with its top
// bit set therefore understates the high word by the other operand, which is added back;
// every sign combination of the two low words is covered here, plus the cross products
// of the high words.
unsigned long long ua;
unsigned long long ub;
long long a;
long long b;

int main() {
    ua = 4294967295ULL;
    ub = 4294967295ULL;
    if (ua * ub != ((4294967294ULL << 32) + 1ULL)) return 0;  // both lows negative as signed

    ua = 2147483648ULL;
    ub = 2ULL;
    if (ua * ub != 4294967296ULL) return 0;  // one low negative, the other positive

    ua = 3000000000ULL;
    ub = 3ULL;
    if (ua * ub != 9000000000ULL) return 0;
    if (ub * ua != 9000000000ULL) return 0;

    ua = 4294967297ULL;  // high word 1, low word 1
    ub = 4294967295ULL;
    if (ua * ub != (0ULL - 1ULL)) return 0;  // high-word cross term

    a = -3LL;
    b = 7LL;
    if (a * b != -21LL) return 0;
    a = -2147483648LL;
    b = -2LL;
    if (a * b != 4294967296LL) return 0;
    a = 3000000000LL;
    b = -3LL;
    if (a * b != -9000000000LL) return 0;
    a = 123456789012LL;
    b = 1000LL;
    if (a * b != 123456789012000LL) return 0;
    return 1;
}
