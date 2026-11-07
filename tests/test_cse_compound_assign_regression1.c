// EXPECT: 1
// Regression test for a LocalCSE bug: a write through a
// CompoundAssign (`arr[i] += n`) must invalidate any cached read of
// that same array recorded earlier, the same way a plain
// `arr[i] = v` already does - LocalCSE's own invalidation switch was
// missing a case for it, so a later repeated read of `arr[0]` could
// silently reuse the pre-write value instead of re-reading it.
int arr[2];
int main() {
    int v;
    arr[0] = 1;
    v = arr[0];     // first "arr[0]" occurrence, recorded
    arr[0] += 10;    // writes arr[0] - must invalidate that recording
    if (v != 1) return 0;
    if (arr[0] != 11) return 0;
    v = arr[0];     // second "arr[0]" occurrence - must re-read, not reuse the stale 1
    if (v != 11) return 0;
    return 1;
}
