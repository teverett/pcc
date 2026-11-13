// EXPECT: 112
// Each half of this pair declares its own `static int shared` and
// `static` function named `helper`, under real separate compilation
// (--format=coff, then --link). `static` gives both internal linkage,
// so the two files' same-named symbols must not collide at link time,
// and each file's own `shared`/`helper` must stay independent of the
// other's - see ../../docs/language.md's "Static declarations".
extern int otherHelper();

static int shared = 10;

static int helper() {
    return shared + 1;  // 11
}

int main() {
    return helper() * 10 + otherHelper();  // 110 + 2 = 112
}
