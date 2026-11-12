// EXPECT: 3002
// Mixed-width members (char then int) in an array of 2: if the
// struct's own size weren't padded up to its widest member's
// alignment, element 1 would overlap element 0's "b" field and this
// would come out wrong.
struct Mixed {
    char a;
    int b;
};

int main() {
    struct Mixed arr[2];
    arr[0].a = 1;
    arr[0].b = 1000;
    arr[1].a = 2;
    arr[1].b = 2000;
    return arr[0].b + arr[1].a + arr[1].b;
}
