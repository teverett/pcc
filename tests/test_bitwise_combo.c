// EXPECT: 1
// Packs four independent 4-bit fields into one word with shifts and
// ORs, then unpacks each one back out with a shift and an AND mask,
// checking every field comes back unchanged.
int main() {
    int packed;
    packed = (1 << 12) | (2 << 8) | (3 << 4) | 4;
    if (((packed >> 12) & 15) != 1) return 0;
    if (((packed >> 8) & 15) != 2) return 0;
    if (((packed >> 4) & 15) != 3) return 0;
    if ((packed & 15) != 4) return 0;
    return 1;
}
