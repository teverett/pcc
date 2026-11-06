// EXPECT: 1
int arr[3];
int idxcalls;

int bumpidx() {
    idxcalls = idxcalls + 1;
    return 1;
}

int main() {
    int x;
    int y;
    int r;
    int *p;
    r = 0;
    x = 5;
    x += 3;      // 8
    if (x != 8) return 100;
    x -= 2;      // 6
    if (x != 6) return 101;
    x *= 4;      // 24
    if (x != 24) return 102;
    x /= 5;      // 4 (24/5 truncating)
    if (x != 4) return 103;
    x %= 3;      // 1
    if (x != 1) return 104;
    x = 6;
    x &= 3;      // 2
    if (x != 2) return 105;
    x |= 5;      // 7
    if (x != 7) return 106;
    x ^= 3;      // 4
    if (x != 4) return 107;
    x <<= 2;     // 16
    if (x != 16) return 108;
    x >>= 3;     // 2
    if (x != 2) return 109;

    x = 5;
    y = x++ + x;  // 5 + 6 = 11
    if (y != 11) return 110;
    x = 5;
    y = ++x + x;  // 6 + 6 = 12
    if (y != 12) return 111;
    x = 5;
    y = x-- - x;  // 5 - 4 = 1
    if (y != 1) return 112;

    arr[0] = 1;
    arr[1] = 2;
    idxcalls = 0;
    arr[bumpidx()] += 10;  // arr[1] = 12, idxcalls must be exactly 1
    if (arr[1] != 12) return 113;
    if (idxcalls != 1) return 114;

    p = &arr[0];
    p++;            // p now points at arr[1]
    if (*p != 12) return 115;
    *p += 1;        // arr[1] = 13
    if (arr[1] != 13) return 116;
    p--;            // p now points at arr[0]
    if (*p != 1) return 117;

    r = (x > 0) ? 42 : -42;
    if (r != 42) return 118;
    r = x > 100 ? 1 : x > 0 ? 2 : 3;
    if (r != 2) return 119;
    // Right-associativity check: `a?b:c?d:e` must mean `a?b:(c?d:e)`,
    // not `(a?b:c)?d:e` - the two give different results exactly when
    // the first condition is true and 'then' is 0: left-associative
    // would wrongly re-test that 0 as the next condition (picking
    // 'e') instead of short-circuiting straight to 'b'.
    r = 1 ? 0 : 1 ? 10 : 20;
    if (r != 0) return 120;

    return 1;
}
