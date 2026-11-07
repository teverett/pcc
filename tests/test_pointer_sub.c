// EXPECT: 3
// Pointer subtraction between two pointers into the same array is
// scaled by the pointee's own size, giving the element distance
// between them rather than the raw address distance.
int arr[5];
int main() {
    int *p;
    int *q;
    p = &arr[1];
    q = &arr[4];
    return q - p;  // element distance: 4-1 = 3
}
