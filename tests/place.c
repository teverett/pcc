// EXPECT: 46
//
// Exercises fixed absolute placement (@addr - see IR.h's
// Slot::hasAddress and each backend's own comment above generate()):
// isr and vec are both pinned to specific addresses instead of
// wherever the backend would otherwise lay them out, and the
// ordinary unplaced code around them (main itself) still has to
// compute the right answer, proving the save/restore (pdp7) and
// forward-padding (8086) mechanisms don't disturb anything else.
//
// The two backends interpret "64"/"128" very differently (pdp7: an
// offset from the link base ".."; 8086: a byte offset from ORG - see
// docs/ir.md's "Fixed absolute placement") - these two particular
// values were chosen to be valid on both: large enough to clear the
// 8086 boot wrapper's own ~20 bytes (so the forward-only padding
// there never goes negative) and comfortably spaced so isr's and
// vec's placed regions don't overlap on either backend.
int isr() @64 {
    return 4;
}

int vec @128;

int main() {
    vec = 42;
    return vec + isr();
}
