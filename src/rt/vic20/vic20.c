#include "vic20.h"

// Staging cell for a KERNAL call's argument/result (see vic20.h's own
// header comment for why this can't just be a local/parameter).
unsigned char rt_a;

void v20_chrout(unsigned char c) {
    rt_a = c;
    asm {
        lda g_rt_a
        jsr 65490  ; CHROUT, $FFD2
    }
}

void v20_puts(char *s) {
    int i;
    i = 0;
    while (s[i] != 0) {
        v20_chrout(s[i]);
        i = i + 1;
    }
}

void v20_crout() {
    v20_chrout(13);
}

void v20_clrscr() {
    v20_chrout(147);
}

unsigned char v20_getin() {
    asm {
        jsr 65508  ; GETIN, $FFE4
        sta g_rt_a
    }
    return rt_a;
}

unsigned char v20_getch() {
    rt_a = 0;
    while (rt_a == 0) {
        rt_a = v20_getin();
    }
    return rt_a;
}

unsigned char v20_checkstop() {
    asm {
        jsr 65505  ; STOP, $FFE1 - sets the Z flag if RUN/STOP is down
        beq v20_stop_pressed
        ldai 0
        jmp v20_stop_done
v20_stop_pressed:
        ldai 1
v20_stop_done:
        sta g_rt_a
    }
    return rt_a;
}
