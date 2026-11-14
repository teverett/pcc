// The Atari 2600 has no OS, no character ROM, and no text display of any
// kind - TIA (the video chip) only ever draws whatever the CPU pokes into
// its own registers, cycle-by-cycle, while the beam is scanning ("racing
// the beam"), which this compiler's ordinary codegen can't promise. So
// the de facto "hello world" here, with no runtime library and no
// display kernel (see docs/platforms.md's own atari2600 entry for why),
// is the simplest thing that's visibly alive on real/emulated hardware:
// poke a single visible color into COLUBK, TIA's background-color
// register, once, then let the halt loop rcc generates after main()
// returns hold it there forever.
unsigned char color;

int main() {
    color = 0x1E;  // a bright, easily-recognized TIA color value
    asm {
        lda g_color
        sta 9  ; COLUBK - TIA's background-color register, $00-$3F mirror
    }
    return 0;
}
