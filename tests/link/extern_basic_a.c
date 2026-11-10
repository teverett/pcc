// EXPECT: 17
// Calls a function and reads a global that this file never defines -
// both declared `extern`, both actually defined in
// extern_basic_b.c's own separately-compiled translation unit, and
// only resolved for real at --link time (see
// ../../docs/language.md's "Extern declarations").
extern int addTen(int x);
extern int shared;

int main() {
    return addTen(shared);  // shared == 7, so 7 + 10 = 17
}
