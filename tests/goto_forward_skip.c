// EXPECT: 1
// A forward goto skips over a statement that would otherwise run.
int main() {
    int result = 1;
    goto skip;
    result = 99;
skip:
    return result;
}
