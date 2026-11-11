// EXPECT: 1
// `&g`/`&arr[i]` must resolve to the exact same address a global
// array's own bare-symbol initializer element resolves to.
int g;
int arr[3];
int *cleanG[1] = {g};
int *cleanArr[1] = {arr};

int main() {
    int *pg;
    int *parr;
    pg = &g;
    parr = &arr[0];
    if (pg != cleanG[0]) return 0;
    if (parr != cleanArr[0]) return 0;
    return 1;
}
