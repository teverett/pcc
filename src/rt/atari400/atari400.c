#include "atari400.h"

// Staging cell for a CIO call's argument/result (see atari400.h's own
// header comment for why this can't just be a local/parameter).
unsigned char rt_a;

void atari_putch(unsigned char c) {
    rt_a = 11;  // PUTCHR
    asm {
        lda g_rt_a
        sta 834  ; ICCOM for IOCB 0, $0342
    }
    rt_a = c;
    asm {
        ldxi 0   ; IOCB 0
        lda g_rt_a
        jsr 58454  ; CIOV, $E456
    }
}

void atari_puts(char *s) {
    int i;
    i = 0;
    while (s[i] != 0) {
        atari_putch(s[i]);
        i = i + 1;
    }
}

void atari_crout() {
    atari_putch(155);  // ATASCII EOL
}

void atari_clrscr() {
    atari_putch(125);  // ATASCII clear screen & home
}

unsigned char atari_getch() {
    rt_a = 7;  // GETCHR
    asm {
        lda g_rt_a
        sta 834  ; ICCOM for IOCB 0, $0342
    }
    asm {
        ldxi 0   ; IOCB 0
        jsr 58454  ; CIOV, $E456
        sta g_rt_a
    }
    return rt_a;
}
