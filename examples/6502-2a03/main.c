// The NES's 2A03 has no memory-map hazard like the Atari 2600's TIA (see
// docs/platforms.md's own 2a03 entry) - but it still has no runtime
// library here, since real NES graphics/sound need the PPU/APU, driven
// cycle-by-cycle in sync with the video signal, which no C compiler's
// ordinary codegen can promise (same reasoning as the 2600's TIA). So
// this "hello world" goes one step further than the 2600's solid-color
// poke: it actually exercises the compiler on real, uncontended 2A03 RAM
// - summing 1..10 in an ordinary C loop - then pokes the result into a
// fixed scratch cell so a test can read it back without needing to know
// where the linker placed `result` itself.
unsigned char result;

int main() {
    unsigned char i;
    unsigned char sum;
    sum = 0;
    for (i = 1; i <= 10; i = i + 1) {
        sum = sum + i;
    }
    result = sum;
    asm {
        lda g_result
        sta 80  ; a free zero-page scratch cell ($0050), clear of both the
                ; pseudo-register region ($0000-$002F) and the frame-
                ; storage bump allocator ($0200-$05FF)
    }
    return 0;
}
