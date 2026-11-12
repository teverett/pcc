// EXPECT: 1
// '@packed' must drop the struct's own trailing size padding: with the
// wide member first and two `char`s after (so no member itself ever
// sits at a misaligned offset - see docs/language.md's own "Structs"
// section for why @packed still rejects a layout that would need one),
// the unpacked size rounds up to a multiple of the `int`'s alignment,
// but the packed size is exactly the member sum.
struct Mixed @packed {
    int b;
    char a;
    char c;
};

int main() {
    if (sizeof(struct Mixed) != sizeof(int) + sizeof(char) + sizeof(char)) return 0;

    struct Mixed m;
    m.b = 1000;
    m.a = 5;
    m.c = 9;
    if (m.b != 1000) return 0;
    if (m.a != 5) return 0;
    if (m.c != 9) return 0;
    return 1;
}
