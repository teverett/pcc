// EXPECT: 1
//
// Exercises this backend's narrower memory access: `char`/`short` get their own `ldb`/`ldh`/
// `stb`/`sth` instructions (SaturnAssembler.cpp), distinct from `int`/pointer's plain 4-byte
// `ld`/`st` - a bug in picking the wrong width would either clobber an adjacent array element or
// read garbage from one. Walks both a `char` array and a `short` array through pointers, writing
// and reading back every element, and separately checks that a `char` and a `short` global sit
// at their own correctly-packed, correctly-sized addresses right next to each other (not each
// silently padded out to a full word).
char bytes[5];
short halves[5];

char g1 = 7;
short g2 = 300;
char g3 = -9;

int sumBytes(char *p, int n) {
    int i;
    int total;
    total = 0;
    for (i = 0; i < n; i = i + 1) {
        total = total + p[i];
    }
    return total;
}

int sumHalves(short *p, int n) {
    int i;
    int total;
    total = 0;
    for (i = 0; i < n; i = i + 1) {
        total = total + p[i];
    }
    return total;
}

int main() {
    int i;
    char *bp;
    short *hp;

    for (i = 0; i < 5; i = i + 1) {
        bytes[i] = i * 10 - 2;   // -2, 8, 18, 28, 38
    }
    bp = &bytes[0];
    if (sumBytes(bp, 5) != 90) return 0;

    for (i = 0; i < 5; i = i + 1) {
        halves[i] = i * 1000 - 100;  // -100, 900, 1900, 2900, 3900
    }
    hp = &halves[0];
    if (sumHalves(hp, 5) != 9500) return 0;

    if (g1 != 7) return 0;
    if (g2 != 300) return 0;
    if (g3 != -9) return 0;

    g1 = g1 + 1;
    g2 = g2 + 1;
    g3 = g3 + 1;
    if (g1 != 8 || g2 != 301 || g3 != -8) return 0;

    return 1;
}
