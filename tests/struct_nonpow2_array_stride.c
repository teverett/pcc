// EXPECT: 33
// A struct of 1/2/3 `char` members (here: size 3) is always a non-power-
// of-two size on every target, regardless of that target's own `int`
// width - unlike struct_mixed_width_fields.c/struct_packed_array.c,
// whose particular struct happens to be exactly 4 bytes (a power of
// two) on a 16-bit-`int` target, this one can't accidentally dodge the
// bug docs/known-bugs.md's "Most backends mis-scale an array index for
// an element whose width isn't 1/2/4/8 bytes" entry describes.
//
// This checks the real pointer stride directly, not a value read back
// through it: a wrong index-scaling can still read back the exact
// value it wrote, because the same wrong address is used consistently
// for both the write and the read-back - see that known-bugs.md entry
// for why a value-based check alone isn't a reliable way to catch this.
struct Three {
    char a;
    char b;
    char c;
};

int main() {
    struct Three arr[3];
    char *p0 = (char *)&arr[0];
    char *p1 = (char *)&arr[1];
    char *p2 = (char *)&arr[2];
    return (p1 - p0) * 10 + (p2 - p1);
}
