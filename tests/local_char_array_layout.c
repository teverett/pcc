// EXPECT: 4
// Local char arrays declared between ints keep to their own storage,
// and a pointer into one walks forward through its elements.
int f() {
    char s[3];
    int x;
    char t[5];
    int y;
    char *p;
    int i;
    int sum;
    int result;

    x = 1234;
    y = -1;
    for (i = 0; i < 3; i = i + 1) {
        s[i] = 100 + i;
    }
    for (i = 0; i < 5; i = i + 1) {
        t[i] = i + 1;
    }
    result = 0;
    if (x == 1234 && y == -1) {
        result = result + 1;
    }
    if (s[0] == 100 && s[2] == 102) {
        result = result + 1;
    }
    p = &t[1];
    sum = 0;
    for (i = 0; i < 4; i = i + 1) {
        sum = sum + *p;
        p = p + 1;
    }
    if (sum == 14) {
        result = result + 1;
    }
    if (&t[4] == &t[0] + 4) {
        result = result + 1;
    }
    return result;
}

int main() {
    return f();
}
