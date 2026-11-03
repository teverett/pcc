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
// Each backend interprets "256"/"384" differently (pdp7: an offset
// from the link base ".."; 8086: a byte offset from ORG - see
// docs/ir.md's "Fixed absolute placement") - these two particular
// values are valid everywhere this test runs: large enough to clear
// every backend's own unplaced code that comes before `isr` in
// program order (on pdp7 specifically, every function - including
// `main` below - gets a real per-call software stack frame, pushing
// even a short function's own generated code well past a couple
// dozen words; see src/codegen/pdp7/Pdp7Codegen.h's own header
// comment), and comfortably spaced so isr's and vec's own placed
// regions never overlap each other or that unplaced code on any
// target.
int isr() @256 {
    return 4;
}

int vec @384;

int main() {
    vec = 42;
    return vec + isr();
}
