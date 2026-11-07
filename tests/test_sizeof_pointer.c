// EXPECT: 1
// sizeof a pointer type (any level) is never smaller than sizeof a
// char, and doesn't change based on what it points to.
int *p;
char *cp;
int main() {
    if (sizeof(int *) < sizeof(char)) return 0;
    if (sizeof(p) != sizeof(cp)) return 0;
    return 1;
}
