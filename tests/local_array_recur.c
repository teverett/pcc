// EXPECT: 23
//
// A Local array (4 elements - more than just index 0) read at
// indices 1 and 2 AFTER a nested recursive call returns. This
// exercises two things a shared-static-storage model gets wrong
// together: each call's own array has to be a fresh, independent
// region (the nested call must not clobber the outer call's own
// arr[1]/arr[2]), and a Local array element's own address has to be
// computed with the right sign relative to its own frame slot (see
// e.g. src/codegen/m65c02/M65C02Codegen.h's own frame-layout comment
// on why a Local array's element i sits at base MINUS i*wordsize,
// the opposite of a Global array's base PLUS i*wordsize).
int g(int n) {
    int arr[4];
    arr[0] = n;
    arr[1] = n + 1;
    arr[2] = n + 2;
    arr[3] = n + 3;
    if (n > 0) {
        g(n - 1);
    }
    return arr[1] + arr[2];  // 11 + 12 = 23, unaffected by any nested call
}

int main() { return g(10); }
