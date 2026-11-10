// EXPECT: 7
// A label doesn't open a new scope of its own in real C - a local
// declared on the statement right after a label must stay visible to
// whatever follows it in the same enclosing block.
int main() {
    goto mid;
mid:
    int x = 7;
    return x;
}
