// EXPECT: 4
// The same as compare_call_rhs_recursion.c with the comparison's 0/1
// value used directly: g(n) counts the levels where n >= g(n - 1),
// which holds at every level since g(n - 1) = n - 1.
int g(int n) {
    int above;
    if (n == 0) {
        return 0;
    }
    above = n >= g(n - 1);
    return above + g(n - 1);
}

int main() {
    return g(4);
}
