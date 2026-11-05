// EXPECT: 1
// Real pointer types: declaring `T*`, taking an address with `&`,
// dereferencing with `*` at the pointee's own type/width, and pointer
// arithmetic scaled by the pointee's own size (see Type.h's own header
// comment on Type::ptrDepth and IRBuilder.cpp's buildPointerBinary).
int arr[5];
char c;
int main() {
    int *p;
    char *cp;
    p = &arr[0];
    *p = 10;
    p = p + 1;
    *p = 20;
    p = p + 3;
    *p = 50;
    if (arr[0] != 10) return 0;
    if (arr[1] != 20) return 0;
    if (arr[4] != 50) return 0;
    if ((p - &arr[0]) != 4) return 0;

    cp = &c;
    *cp = 7;
    if (*cp != 7) return 0;
    if (c != 7) return 0;

    return 1;
}
