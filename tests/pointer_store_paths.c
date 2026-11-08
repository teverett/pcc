// EXPECT: 4
// Stores and in-place updates through pointers and array elements,
// with and without a call in the stored value, including a postfix ++
// that yields the old value.
int g[6];
int calls;

int seven() {
    calls = calls + 1;
    return 7;
}

int main() {
    int *p;
    int old;
    int result;

    result = 0;
    g[2] = 16;
    old = g[2]++;
    result = result + (old == 16 && g[2] == 17);
    p = &g[4];
    *p = seven() + 1;
    result = result + (g[4] == 8 && calls == 1);
    *(p + 1) = 9;
    result = result + (g[5] == 9);
    *p -= seven();
    result = result + (g[4] == 1);
    return result;
}
