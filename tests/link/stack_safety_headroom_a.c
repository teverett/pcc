// EXPECT: 7
// Regression check for the pdp7 linker's stack-safety margin
// (Pdp7LinkerFactory.cpp's kStackReserve, added alongside
// kAddressFieldLimit): a real cross-file link whose own highest-
// addressed symbol still leaves comfortable headroom below the
// runtime stack's own start (__sp, 8191 - see
// Pdp7LinkerFactory.cpp's own header comment on it) must keep
// linking and running correctly - the fix must not refuse a program
// just for being nontrivially sized, only one that's actually too
// close to the top. `tailMarker` is defined in
// stack_safety_headroom_b.c, after a filler array sized to leave
// well over kStackReserve (512) words of headroom once linked after
// this file and the shared entry object - see that file's own
// header, and stack_safety_overflow_a.c/_b.c (not part of this
// EXPECT-based suite - see tests/run_stack_safety_tests.sh) for the
// sibling case that proves a much bigger filler, same shape, gets
// refused instead.
extern int tailMarker;
extern int setTailMarker(int v);

int main() {
    setTailMarker(7);
    return tailMarker;
}
