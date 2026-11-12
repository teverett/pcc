// EXPECT: 1
// sizeof(T) checked against relationships the layout algorithm
// guarantees, not any one target's own concrete byte counts (see
// types_sizeof.c's own header comment) - this holds the same way on
// every backend.
struct Mixed {
    char a;
    int b;
};
struct Aligned {
    int a;
    int b;
};

int main() {
    // Two equal-width int members need no padding.
    if (sizeof(struct Aligned) != sizeof(int) + sizeof(int)) return 0;
    // Padding only ever adds space, never removes it: a struct is at
    // least as big as the sum of its members (struct_mixed_width_fields.c
    // separately exercises, at runtime, that the padding this adds is
    // actually enough to keep an array of these elements aligned).
    if (sizeof(struct Mixed) < sizeof(char) + sizeof(int)) return 0;
    if (sizeof(struct Mixed) < sizeof(int)) return 0;
    return 1;
}
