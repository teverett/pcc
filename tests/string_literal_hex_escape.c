// EXPECT: 65
//
// A string literal's hex escape (\xhh..., one or more hex digits - see
// grammar/CLexer.g4's own STRINGLIT_ESC) decodes to that byte value,
// same spelling as ISO C's; a value wider than a byte truncates to its
// low 8 bits (also same as ISO C's own escape-overflow behavior), so
// "\x141" is 0x141 (321) truncated to 0x41 ('A', 65), not 'A' followed
// by a literal '1'.
int main() {
    char *s = "\x141";
    return s[0];
}
