// EXPECT: 42
// A Local array's element i lives at frameOffset+i, so its address is
// the array's own base address MINUS i words, not plus - the mirror
// image of a Global array's increasing-address layout. This checks a
// 4-element Local array's non-zero-index elements, inside a function
// that also recurses, so the array's own frame also has to be fresh
// on every call.
int sumSkip(int n) {
    int arr[4];
    arr[0] = 100;
    arr[1] = 10;
    arr[2] = 20;
    arr[3] = 12;
    if (n > 0) {
        sumSkip(n - 1);
    }
    return arr[1] + arr[2] + arr[3];
}

int main() {
    return sumSkip(3);
}
