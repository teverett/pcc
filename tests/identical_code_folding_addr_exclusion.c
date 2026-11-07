// EXPECT: 11
// Exercises IdenticalCodeFolding.cpp's `@addr` exclusion: ret4Pinned and
// ret4Plain have identical bodies, but ret4Pinned is pinned to a fixed
// address and must keep its own definition rather than being folded away
// into (or replacing) ret4Plain - both must still run as independently
// callable functions.
int ret4Pinned() @256 {
    return 4;
}
int ret4Plain() {
    return 4;
}
int main() {
    return ret4Pinned() + ret4Plain() + 3;  // 4 + 4 + 3 = 11
}
