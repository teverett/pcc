#include "apple2.h"

// Staging cell for a ROM call's argument/result (see apple2.h's own header
// comment for why this can't just be a local/parameter).
unsigned char rt_a;

void a2_cout(unsigned char c) {
    // COUT reads the high bit as "normal, unshifted display text", not
    // part of the character code - a byte with bit 7 clear is a control
    // code or inverse/flash text instead, depending on ROM revision - so
    // every ordinary character needs it set.
    rt_a = c | 128;
    asm {
        lda g_rt_a
        jsr 65005  ; COUT, $FDED
    }
}

void a2_puts(char *s) {
    int i;
    i = 0;
    while (s[i] != 0) {
        a2_cout(s[i]);
        i = i + 1;
    }
}

void a2_crout() {
    asm {
        jsr 64910  ; CROUT, $FD8E
    }
}

unsigned char a2_rdkey() {
    asm {
        jsr 64780  ; RDKEY, $FD0C
        sta g_rt_a
    }
    return rt_a;
}

void a2_home() {
    asm {
        jsr 64600  ; HOME, $FC58
    }
}

void a2_prbyte(unsigned char b) {
    rt_a = b;
    asm {
        lda g_rt_a
        jsr 64986  ; PRBYTE, $FDDA
    }
}

void a2_prhex(unsigned char n) {
    rt_a = n;
    asm {
        lda g_rt_a
        jsr 64995  ; PRHEX, $FDE3
    }
}

void a2_bell() {
    asm {
        jsr 65338  ; BELL, $FF3A
    }
}

void a2_wait(unsigned char n) {
    rt_a = n;
    asm {
        lda g_rt_a
        jsr 64680  ; WAIT, $FCA8
    }
}

// ProDOS MLI call trampoline (see ProDOS Technical Reference Manual ch. 5):
// JSR $BF00, the command byte, then the parameter-list address - all three
// have to sit in the instruction stream immediately after the JSR, so
// mli_call() builds this once and patches bytes 3-5 before every call
// instead of trying to pass them as ordinary C arguments/registers.
// Byte layout: 32,0,191 = JSR $BF00; byte 3 = cmd; bytes 4-5 = paramsAddr
// lo/hi; 96 = RTS (returns to mli_call()'s own caller).
unsigned char mli_trampoline[7] = {32, 0, 191, 0, 0, 0, 96};

void mli_patch(unsigned char cmd, int paramsAddr) {
    mli_trampoline[3] = cmd;
    mli_trampoline[4] = paramsAddr & 255;
    mli_trampoline[5] = (paramsAddr >> 8) & 255;
}

int mli_call(unsigned char cmd, int paramsAddr) {
    mli_patch(cmd, paramsAddr);
    asm {
        jsr g_mli_trampoline
        sta g_rt_a
    }
    return rt_a;
}
