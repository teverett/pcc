// EXPECT: 3
// Unsigned division in a separately compiled file links against the
// runtime's unsigned routine and treats the top bit as magnitude:
// (UMAX-5) / (UMAX-6) is 1 remainder 1, and (UMAX-5) / 2 is exact.
extern unsigned int uquot(unsigned int a, unsigned int b);
extern unsigned int urem(unsigned int a, unsigned int b);

int main() {
    int result;
    unsigned int big;
    result = 0;
    big = 0u - 6;
    result = result + (uquot(big, 0u - 7) == 1);
    result = result + (urem(big, 0u - 7) == 1);
    result = result + (uquot(big, 2) + uquot(big, 2) == big);
    return result;
}
