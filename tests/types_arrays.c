// EXPECT: 1
//
// An array's own element type controls its index scaling and storage
// layout, instead of a fixed word-size scale - a `char` array is
// byte-packed, a `long` array is word-packed, and indexing each has
// to scale by the right amount or this reads back the wrong element
// entirely. Kept to 2 elements per array: every `long` array access
// here is a wide (`long`) operation, and this backend's own boot-
// sector output (--format=raw) has a hard 512-byte budget (see
// I8086Codegen.h's own header comment) that more elements would
// overflow.
char bytes[2];
long words[2];
int main() {
    bytes[0] = 10;
    bytes[1] = 20;
    if (bytes[0] != 10) return 0;
    if (bytes[1] != 20) return 0;

    words[0] = 100000;
    words[1] = 200000;
    if (words[0] != 100000) return 0;
    if (words[1] != 200000) return 0;

    return 1;
}
