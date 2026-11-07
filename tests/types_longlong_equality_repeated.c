// EXPECT: 1
// Several `long long` equality/inequality comparisons, mixed with
// wide add/subtract, in the same function - exercises
// emitWideEqualityCompare()'s own shared byte-scan subroutine (see
// ../src/codegen/i8008/I8008Codegen.cpp's own
// getWideEqualitySubroutine() comment) being correctly `cal`-ed from
// several different call sites, and pushWide()/popWide() correctly
// saving/restoring a `long long` operand across each of those
// comparisons and arithmetic ops in turn (each one pushes one operand
// while evaluating the other, popping it back before combining them -
// see pushWide()'s own comment).
int main() {
    long long a;
    long long b;
    long long c;

    a = 100000LL;
    b = 100000LL;
    if (a != b) return 100;
    if (a == b + 1LL) return 101;

    c = a + b;
    if (c != 200000LL) return 102;

    c = c - a;
    if (c != b) return 103;

    return 1;
}
