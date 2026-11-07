// EXPECT: 1
// `long` arithmetic whose result exceeds a 16-bit int's own range but
// fits comfortably within long's own wider width on every backend.
int main() {
    long a;
    a = 100000L + 100000L;
    if (a != 200000L) return 0;
    return 1;
}
