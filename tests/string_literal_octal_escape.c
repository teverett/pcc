// EXPECT: 198
//
// A string literal's octal escape (\ooo, 1-3 octal digits - see
// grammar/CLexer.g4's own STRINGLIT_ESC) decodes to that byte value,
// same spelling as ISO C's. \101 \102 \103 are 'A' 'B' 'C' (65 66 67);
// \0 still works too, as the special case of a 1-digit octal escape,
// and ends the string the same way a real NUL byte would for anything
// reading it as a C string - the two bytes after it are never reached.
int main() {
    char *s = "\101\102\103\0unreached";
    return s[0] + s[1] + s[2];
}
