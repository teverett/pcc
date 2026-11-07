// EXPECT: 1
// `unsigned long` arithmetic whose operands and result all sit past
// what a signed 32-bit value could hold, but well within unsigned
// long's own range.
int main() {
    unsigned long a;
    a = 4000000000UL - 1000000000UL;  // 3000000000
    if (a != 3000000000UL) return 0;
    return 1;
}
