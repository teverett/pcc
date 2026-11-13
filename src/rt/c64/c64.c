#include "c64.h"

// Staging cell for a KERNAL call's argument/result (see c64.h's own
// header comment for why this can't just be a local/parameter).
unsigned char rt_a;

void c64_chrout(unsigned char c) {
    rt_a = c;
    asm {
        lda g_rt_a
        jsr 65490  ; CHROUT, $FFD2
    }
}

void c64_puts(char *s) {
    int i;
    i = 0;
    while (s[i] != 0) {
        c64_chrout(s[i]);
        i = i + 1;
    }
}

void c64_crout() {
    c64_chrout(13);
}

void c64_clrscr() {
    c64_chrout(147);
}

unsigned char c64_getin() {
    asm {
        jsr 65508  ; GETIN, $FFE4
        sta g_rt_a
    }
    return rt_a;
}

unsigned char c64_getch() {
    rt_a = 0;
    while (rt_a == 0) {
        rt_a = c64_getin();
    }
    return rt_a;
}

unsigned char c64_checkstop() {
    asm {
        jsr 65505  ; STOP, $FFE1 - sets the Z flag if RUN/STOP is down
        beq c64_stop_pressed
        ldai 0
        jmp c64_stop_done
c64_stop_pressed:
        ldai 1
c64_stop_done:
        sta g_rt_a
    }
    return rt_a;
}
