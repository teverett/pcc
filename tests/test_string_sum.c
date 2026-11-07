// EXPECT: 198
// A string literal decays to a pointer to its own first character;
// indexing through it reads each byte in turn, same as any other
// char pointer.
int main() {
    char *s;
    s = "ABC";
    return s[0] + s[1] + s[2];  // 65+66+67 = 198
}
