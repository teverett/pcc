// EXPECT: 110
int x;
int arr[4];

int sum3(int p) {
    // p[i], not *(p+i): raw pointer arithmetic is unscaled address
    // math (see README's "Scope and limitations" - it varies with
    // the target's word-addressing), while p[i] is defined to mean
    // the i'th element regardless of target, the same way arr[i] is.
    return p[0] + p[1] + p[2];
}

int main() {
    int px;
    int result;

    x = 42;
    px = &x;
    *px = 100;  // modifies x via pointer

    arr[0] = 1;
    arr[1] = 2;
    arr[2] = 3;
    arr[3] = 4;

    result = x;                       // 100
    result = result + sum3(&arr[0]);  // +1+2+3 = 106
    result = result + *(&arr[3]);     // +4 = 110
    return result;
}
