// `filler`'s own size (1000 words) is deliberately small enough to
// leave well over kStackReserve (512) words of headroom below the
// runtime stack's own start at 8191 once linked after
// stack_safety_headroom_a.c and the shared entry object - confirmed
// directly (highest linked symbol lands hundreds of words below the
// new, tighter limit). `setTailMarker` exists only so
// stack_safety_headroom_a.c's own write to `tailMarker` crosses a
// real object-file boundary too, the same shape as the real bug this
// margin was added for (see Pdp7LinkerFactory.cpp's own header
// comment on kStackReserve).
int filler[1000];
int tailMarker;

int setTailMarker(int v) {
    tailMarker = v;
    return 0;
}
