// EXPECT: 103
//
// 'enum Tag' resolves to plain 'int' (this subset gives every enum the
// same underlying type, with no separate runtime representation - see
// docs/language.md's "Enums"), so sizeof(enum Tag) always equals
// sizeof(int) (checked via a comparison rather than hardcoding either
// width, since int's own width varies by target), and an enum constant
// participates in ordinary arithmetic exactly like any other int
// literal would.
enum Flag { OFF = 0, ON = 1 };

int main() {
    enum Flag f = ON;
    int ok = (sizeof(enum Flag) == sizeof(int)) ? 1 : 0;
    return ok * 100 + f * 3 + OFF;
}
