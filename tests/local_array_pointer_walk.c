// EXPECT: 3
// A local array's elements ascend in memory, so a pointer to one of
// them walks forward through the rest, both for an array filled by
// assignment and for one with an initializer.
int sum(int *p, int n) {
    int s;
    s = 0;
    while (n > 0) {
        s = s + *p;
        p = p + 1;
        n = n - 1;
    }
    return s;
}

int main() {
    int a[4];
    int b[3] = {5, 6, 7};
    int result;

    a[0] = 1;
    a[1] = 2;
    a[2] = 3;
    a[3] = 4;
    result = 0;
    result = result + (sum(&a[0], 4) == 10);
    result = result + (sum(&b[1], 2) == 13);
    result = result + (*(&b[0] + 2) == 7);
    return result;
}
