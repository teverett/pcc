// EXPECT: 75
// A local array's elements at index 1 and 2 (not just index 0) read
// back the values stored there, independent of each other and of
// element 3.
int f() {
    int a[4];
    a[0] = 5;
    a[1] = 15;
    a[2] = 25;
    a[3] = 35;
    return a[1] + a[2] + a[3];
}

int main() {
    return f();
}
