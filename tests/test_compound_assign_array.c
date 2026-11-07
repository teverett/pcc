// EXPECT: 1
// Compound assignment through an array index combines with that
// element's own current value, the same way a plain scalar's +=
// does.
int arr[3];
int main() {
    arr[0] = 5;
    arr[1] = 10;
    arr[2] = 20;
    arr[0] += 3;   // 8
    arr[1] -= 4;   // 6
    arr[2] *= 2;   // 40
    if (arr[0] != 8) return 0;
    if (arr[1] != 6) return 0;
    if (arr[2] != 40) return 0;
    return 1;
}
