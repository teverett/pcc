// EXPECT: 1
// An array of a packed struct must stride by the packed (unpadded)
// size, not the naturally-aligned one - otherwise element 1's fields
// land on the wrong bytes. Member order (wide member first, then two
// `char`s) keeps every member's own offset aligned, so this doesn't
// hit the misaligned-offset rejection @packed enforces - see
// struct_packed_basic.c's own header comment.
struct Mixed @packed {
    int b;
    char a;
    char c;
};

int main() {
    struct Mixed arr[2];
    arr[0].b = 10;
    arr[0].a = 1;
    arr[0].c = 2;
    arr[1].b = 20;
    arr[1].a = 3;
    arr[1].c = 4;

    if (arr[0].b != 10) return 0;
    if (arr[0].a != 1) return 0;
    if (arr[0].c != 2) return 0;
    if (arr[1].b != 20) return 0;
    if (arr[1].a != 3) return 0;
    if (arr[1].c != 4) return 0;

    char *base = (char *)&arr[0];
    char *second = (char *)&arr[1];
    if (second - base != sizeof(struct Mixed)) return 0;

    return 1;
}
