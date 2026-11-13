// EXPECT: 313
//
// Members with no '=' count up from 0 (RED=0, GREEN=1); an explicit
// value (BLUE=10) resumes counting from there (PURPLE=11). An enum
// constant is usable anywhere an int constant is: a bare expression
// (the switch's own subject and GREEN below), a 'case' label, a
// static/global initializer, and a function parameter/variable
// declared with the enum's own type (which is just 'int' underneath -
// see docs/language.md's "Enums").
enum Color { RED, GREEN, BLUE = 10, PURPLE };

int classify(enum Color c) {
    switch (c) {
        case RED: return 1;
        case GREEN: return 2;
        case BLUE: return 3;
        case PURPLE: return 4;
        default: return 0;
    }
}

static enum Color g_favorite = BLUE;

int main() {
    int x = GREEN;
    return classify(g_favorite) * 100 + classify(x) + PURPLE;
}
