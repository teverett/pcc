// EXPECT: 1
// A leading-zero literal parses as octal (ISO C), and a bare "0" still
// parses as the integer zero rather than being misread as an octal
// prefix with no digits.
int main() {
    int a = 010;
    int zero = 0;
    if (a != 8) return 0;
    if (zero != 0) return 0;
    return 1;
}
