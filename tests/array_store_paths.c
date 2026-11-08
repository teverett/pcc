// EXPECT: 4
// Array elements reached with constant and computed indices, stored
// to with and without a call in the stored value, in a global and a
// local array.
int g[6];
int calls;

int seven() {
    calls = calls + 1;
    return 7;
}

int main() {
    int l[4];
    int i;
    int result;

    result = 0;
    i = 2;
    g[0] = 5;
    g[i] = g[0] + 1;
    l[3] = seven();
    l[i] = l[3] * 2;
    result = result + (g[2] == 6 && l[3] == 7 && l[2] == 14);
    l[0] = 3;
    l[1] = l[0] + l[3];
    result = result + (l[1] == 10);
    g[i] += 10;
    l[i] |= 1;
    result = result + (g[2] == 16 && l[2] == 15);
    result = result + (calls == 1);
    return result;
}
