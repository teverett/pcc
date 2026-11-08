// EXPECT: 2
// A call to a function declared with an `extern` prototype converts
// each argument to the declared parameter type: on a target whose int
// can't hold 213568 the literal is a long, and it still arrives as the
// one-word unsigned int the definition expects.
extern int same(unsigned int x, unsigned int y);

int main() {
    int result;
    result = 0;
    result = result + same(213568, 213568);
    result = result + same(7, 7);
    return result;
}
