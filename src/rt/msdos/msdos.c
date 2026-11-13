#include "msdos.h"

// Staging cells for an INT 21h call's argument/result words (see msdos.h's
// own header comment for why these can't just be locals/parameters, and
// why none of this is reentrant). rt_w holds the primary result; rt_w2-4
// hold up to three more arguments a single call needs staged before its
// asm { } block runs.
int rt_w;
int rt_w2;
int rt_w3;
int rt_w4;

// Shared I/O staging buffer for dos_read()/dos_write()/dos_print() - see
// dos_buf_get()/dos_buf_set() below for why this isn't exposed directly.
unsigned char rt_buf[256];
// ASCIIZ staging for a path argument (dos_create/dos_open/dos_delete, and
// both of dos_rename's own paths) - copied in by copy_name() below, since
// DX/DI need a fixed label's address, not a runtime pointer value.
char rt_name[128];
char rt_name2[128];
// INT 21h/0Ah's own fixed buffer layout: byte 0 = max chars in (set by the
// caller), byte 1 = actual chars read (set by DOS), bytes 2.. = the chars
// themselves plus a trailing CR DOS adds but dos_gets() below strips.
unsigned char rt_line[258];

void copy_name(char *dst, char *src) {
    int i;
    i = 0;
    while (src[i] != 0 && i < 127) {
        dst[i] = src[i];
        i = i + 1;
    }
    dst[i] = 0;
}

unsigned char dos_buf_get(unsigned char i) {
    return rt_buf[i];
}

void dos_buf_set(unsigned char i, unsigned char value) {
    rt_buf[i] = value;
}

void dos_putchar(unsigned char c) {
    rt_w = c;
    asm {
        mov dx, [g_rt_w]  ; dl = c, dh = 0 (INT 21h/02h only reads dl)
        mov ax, 0x0200    ; ah = 2 (write character)
        int 0x21
    }
}

void dos_puts(char *s) {
    int i;
    i = 0;
    while (s[i] != 0) {
        dos_putchar(s[i]);
        i = i + 1;
    }
}

void dos_print(char *s) {
    int i;
    i = 0;
    while (s[i] != 0 && i < 255) {
        rt_buf[i] = s[i];
        i = i + 1;
    }
    rt_buf[i] = '$';
    asm {
        mov dx, g_rt_buf
        mov ax, 0x0900    ; ah = 9 (write '$'-terminated string)
        int 0x21
    }
}

unsigned char dos_getchar() {
    asm {
        mov ax, 0x0100    ; ah = 1 (read character, with echo)
        int 0x21
        mov [g_rt_w], ax  ; al = the character read; ah is whatever DOS left there
    }
    return rt_w;  // truncates to the real al byte
}

unsigned char dos_gets(char *buf, unsigned char maxlen) {
    int i;
    rt_line[0] = maxlen;
    asm {
        mov dx, g_rt_line
        mov ax, 0x0A00    ; ah = 0x0A (buffered keyboard input)
        int 0x21
    }
    i = 0;
    while (i < rt_line[1]) {
        buf[i] = rt_line[i + 2];
        i = i + 1;
    }
    buf[i] = 0;
    return rt_line[1];
}

unsigned char dos_kbhit() {
    asm {
        mov ax, 0x0B00    ; ah = 0x0B (check standard-input status)
        int 0x21
        and ax, 255       ; al is 0xFF (ready) or 0x00 (not ready); ah is undefined
        mov [g_rt_w], ax
    }
    return rt_w != 0;
}

void dos_exit(unsigned char code) {
    // This C-level grammar has no hex-literal syntax (CLexer.g4's NUMBER is
    // decimal-only, unlike the asm-text grammar's own 8086-style hex), so
    // 0x4C00 is written out as 19456: ah = 0x4C (terminate), al = code -
    // safe to just add since code < 256 can never carry into ah's byte.
    rt_w = 19456 + code;
    asm {
        mov ax, [g_rt_w]
        int 0x21
    }
}

int dos_create(char *path, int attrs) {
    copy_name(&rt_name[0], path);
    rt_w2 = attrs;
    asm {
        mov cx, [g_rt_w2]  ; file attribute byte (0 = normal)
        mov dx, g_rt_name
        mov ax, 0x3C00     ; ah = 0x3C (create/truncate file)
        int 0x21
        jnc DOS_CREATE_OK
        neg ax             ; negative = -(real DOS error code)
DOS_CREATE_OK:
        mov [g_rt_w], ax
    }
    return rt_w;
}

int dos_open(char *path, unsigned char mode) {
    copy_name(&rt_name[0], path);
    rt_w2 = mode;
    asm {
        mov dx, g_rt_name
        mov si, [g_rt_w2]
        mov ax, 0x3D00     ; ah = 0x3D (open file); al = access mode
        add ax, si
        int 0x21
        jnc DOS_OPEN_OK
        neg ax
DOS_OPEN_OK:
        mov [g_rt_w], ax
    }
    return rt_w;
}

int dos_close(int handle) {
    rt_w2 = handle;
    asm {
        mov bx, [g_rt_w2]
        mov ax, 0x3E00     ; ah = 0x3E (close file)
        int 0x21
        jnc DOS_CLOSE_OK
        neg ax
        mov [g_rt_w], ax
        jmp DOS_CLOSE_DONE
DOS_CLOSE_OK:
        mov ax, 0          ; AX isn't a meaningful "success" value here - 0 is this wrapper's own convention
        mov [g_rt_w], ax
DOS_CLOSE_DONE:
    }
    return rt_w;
}

int dos_read(int handle, unsigned char count) {
    rt_w2 = handle;
    rt_w3 = count;
    asm {
        mov bx, [g_rt_w2]
        mov cx, [g_rt_w3]
        mov dx, g_rt_buf
        mov ax, 0x3F00     ; ah = 0x3F (read file/device)
        int 0x21
        jnc DOS_READ_OK
        neg ax
DOS_READ_OK:
        mov [g_rt_w], ax
    }
    return rt_w;
}

int dos_write(int handle, unsigned char count) {
    rt_w2 = handle;
    rt_w3 = count;
    asm {
        mov bx, [g_rt_w2]
        mov cx, [g_rt_w3]
        mov dx, g_rt_buf
        mov ax, 0x4000     ; ah = 0x40 (write file/device)
        int 0x21
        jnc DOS_WRITE_OK
        neg ax
DOS_WRITE_OK:
        mov [g_rt_w], ax
    }
    return rt_w;
}

int dos_delete(char *path) {
    copy_name(&rt_name[0], path);
    asm {
        mov dx, g_rt_name
        mov ax, 0x4100     ; ah = 0x41 (delete file)
        int 0x21
        jnc DOS_DELETE_OK
        neg ax
        mov [g_rt_w], ax
        jmp DOS_DELETE_DONE
DOS_DELETE_OK:
        mov ax, 0
        mov [g_rt_w], ax
DOS_DELETE_DONE:
    }
    return rt_w;
}

int dos_rename(char *oldpath, char *newpath) {
    copy_name(&rt_name[0], oldpath);
    copy_name(&rt_name2[0], newpath);
    asm {
        mov dx, g_rt_name
        mov di, g_rt_name2
        mov ax, 0x5600     ; ah = 0x56 (rename file)
        int 0x21
        jnc DOS_RENAME_OK
        neg ax
        mov [g_rt_w], ax
        jmp DOS_RENAME_DONE
DOS_RENAME_OK:
        mov ax, 0
        mov [g_rt_w], ax
DOS_RENAME_DONE:
    }
    return rt_w;
}

int dos_seek(int handle, int offset, unsigned char method) {
    rt_w2 = handle;
    rt_w3 = offset;
    rt_w4 = method;
    asm {
        mov bx, [g_rt_w2]
        mov dx, [g_rt_w3]  ; offset low word
        mov cx, 0          ; offset high word - always 0 (16-bit offsets only)
        mov si, [g_rt_w4]
        mov ax, 0x4200     ; ah = 0x42 (move file pointer); al = method
        add ax, si
        int 0x21
        jnc DOS_SEEK_OK
        neg ax
DOS_SEEK_OK:
        mov [g_rt_w], ax   ; new position's low word (high word, dx, is dropped)
    }
    return rt_w;
}

int dos_date_year() {
    asm {
        mov ax, 0x2A00    ; ah = 0x2A (get date)
        int 0x21
        mov [g_rt_w], cx  ; cx = year, whole word, no extraction needed
    }
    return rt_w;
}

unsigned char dos_date_month() {
    asm {
        mov ax, 0x2A00
        int 0x21
        mov ax, dx        ; dh = month, dl = day
        mov cx, 8
        shr ax, cl        ; ax = dh
        mov [g_rt_w], ax
    }
    return rt_w;
}

unsigned char dos_date_day() {
    asm {
        mov ax, 0x2A00
        int 0x21
        mov ax, dx
        and ax, 255       ; ax = dl
        mov [g_rt_w], ax
    }
    return rt_w;
}

unsigned char dos_date_weekday() {
    asm {
        mov ax, 0x2A00
        int 0x21
        and ax, 255       ; al = weekday; ah is undefined
        mov [g_rt_w], ax
    }
    return rt_w;
}

unsigned char dos_time_hour() {
    asm {
        mov ax, 0x2C00    ; ah = 0x2C (get time)
        int 0x21
        mov ax, cx        ; ch = hour, cl = minute
        mov cx, 8
        shr ax, cl        ; ax = ch
        mov [g_rt_w], ax
    }
    return rt_w;
}

unsigned char dos_time_minute() {
    asm {
        mov ax, 0x2C00
        int 0x21
        mov ax, cx
        and ax, 255       ; ax = cl
        mov [g_rt_w], ax
    }
    return rt_w;
}

unsigned char dos_time_second() {
    asm {
        mov ax, 0x2C00
        int 0x21
        mov ax, dx        ; dh = second, dl = hundredths
        mov cx, 8
        shr ax, cl        ; ax = dh
        mov [g_rt_w], ax
    }
    return rt_w;
}

unsigned char dos_time_hundredths() {
    asm {
        mov ax, 0x2C00
        int 0x21
        mov ax, dx
        and ax, 255       ; ax = dl
        mov [g_rt_w], ax
    }
    return rt_w;
}

unsigned char dos_cmdline_len() {
    asm {
        movb ax, [0x80]   ; PSP+0x80: command-tail length byte, absolute
                           ; (a .com program's own DS already is the PSP
                           ; segment, so no segment override is needed)
        mov [g_rt_w], ax
    }
    return rt_w;
}

unsigned char dos_cmdline_char(unsigned char i) {
    rt_w2 = i;
    asm {
        mov bx, [g_rt_w2]
        movb ax, [bx+129]  ; PSP+0x81 (129): the command-tail text itself
        mov [g_rt_w], ax
    }
    return rt_w;
}
