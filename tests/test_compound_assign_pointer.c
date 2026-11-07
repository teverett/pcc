// EXPECT: 1
// Compound assignment through a dereferenced pointer combines with
// the pointee's own current value and writes the result back
// through that same pointer.
int x;
int main() {
    int *p;
    x = 10;
    p = &x;
    *p += 5;   // 15
    *p *= 2;   // 30
    *p -= 10;  // 20
    if (x != 20) return 0;
    return 1;
}
