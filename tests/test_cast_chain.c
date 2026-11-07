// EXPECT: 1
// A value cast down to a narrower type and back up again round-trips
// exactly when it already fit in the narrower type to begin with.
int main() {
    int a;
    char c;
    int b;
    a = 100;
    c = (char)a;
    b = (int)c;
    if (b != 100) return 0;
    return 1;
}
