#ifndef RCC_RT_MSDOS_H
#define RCC_RT_MSDOS_H

// MS-DOS runtime library for rcc's `--target=8086-dos` backend.
//
// Calls DOS directly via `INT 21h` (see Ralf Brown's Interrupt List for the
// function numbers used below). I8086Assembler.cpp's `mov` only ever
// targets a full 16-bit register (no `ah`/`al`/`dl`/etc. sub-register names
// exist at all - see its own `regIndex()`), so a function number that has
// to land in `ah` alone, or a one-byte argument/result that has to land in
// `al`/`dl` alone, is always combined as a full 16-bit immediate
// (`mov ax, 0x3D00`) or built up with a register add (`add ax, si`) - the
// same trick `I8086LinkerFactory.cpp`'s/`I8086Codegen.cpp`'s own
// `mov ax, 0x4C00` program-exit sequence already uses. A DOS field packed
// into one byte of a register pair (date/time's month/day/hour/second) is
// extracted with `and ax, 255` (low byte) or `mov cx, 8` + `shr ax, cl`
// (high byte), since there's no `ah`/`dh`-style access either.
//
// Every value staged between C and an `asm { }` block goes through a
// global, never a local/parameter - a local/parameter has no fixed
// address an asm block's literal text could name directly, only a global
// does (see apple2.c's own header comment for the same reason on 6502).
// Every staging global is read/written through `[g_name]` inside asm,
// never bare `g_name` - a bare label is this dialect's own "load this
// address as an immediate" operand (e.g. `mov dx, g_dos_buf`, used to
// point DOS at a fixed buffer), not a memory reference; only the bracketed
// form dereferences it.
//
// None of these wrappers are reentrant (they all share the same staging
// globals) - fine for a single-threaded bare DOS program, the only kind
// this target runs.
//
// `dos_open`/`dos_create`/`dos_close`/`dos_read`/`dos_write`/`dos_delete`/
// `dos_rename`/`dos_seek` return a non-negative result on success and a
// negated DOS error code (`-AX`) on failure - check `< 0`, not `== 0`,
// since legitimate results (and handle 0) can themselves be 0.
//
// Deliberately not provided:
// - Environment-block access: a .com process's PSP (byte 0x2C) holds only
//   the environment's real-mode *segment*, and reading its strings needs
//   a segment override (`mov ax, es:[bx]`) this dialect's own assembler
//   has no syntax for at all (`I8086Assembler.cpp` never emits one) - so
//   there's no way to dereference that segment from here.
// - `INT 21h`/48h-4Ah (allocate/free/resize memory): 48h returns a real-
//   mode *segment* in AX, same dereferencing problem as the environment
//   block above - a wrapper that just handed back an unusable segment
//   number wouldn't be worth having.
//
// Include this file directly (`#include "msdos.c"`) for a single-file
// `--format=raw` build, or compile msdos.c on its own and `--link` it
// against a separately-compiled program (`--format=coff`/`elf`/`aout`).

// Writes one character via INT 21h/02h.
void dos_putchar(unsigned char c);
// Writes a NUL-terminated string one character at a time via dos_putchar()
// - '\n' is sent as-is (DOS's console driver doesn't add a '\r' for you;
// send "\r\n" for a real new line).
void dos_puts(char *s);
// Writes a NUL-terminated string in one INT 21h/09h call instead of
// dos_puts()'s character-at-a-time loop. Two real DOS limits apply: 09h's
// own string has to end with '$', so a literal '$' inside `s` truncates
// the output there (not a bug in this wrapper - DOS's own convention), and
// the string is staged through the same internal buffer dos_read()/
// dos_write() use first, so anything past the first 255 characters of
// `s` is silently dropped.
void dos_print(char *s);
// Reads one character via INT 21h/01h (echoed to the console by DOS itself).
unsigned char dos_getchar();
// Reads a line into `buf` (NUL-terminated, the trailing CR dropped) via
// INT 21h/0Ah, at most `maxlen` characters (not counting the NUL) - `buf`
// must be at least `maxlen + 1` bytes. Returns the number of characters
// actually read.
unsigned char dos_gets(char *buf, unsigned char maxlen);
// True (nonzero) if a key is waiting to be read, via INT 21h/0Bh -
// doesn't block and doesn't consume the key.
unsigned char dos_kbhit();
// Terminates the program via INT 21h/4Ch, with `code` as the real DOS exit
// code (readable from a batch file's own `ERRORLEVEL`). Never returns.
void dos_exit(unsigned char code);

// Reads/writes byte `i` of the shared 256-byte I/O buffer dos_read()/
// dos_write() transfer through - accessors rather than a plain exported
// array, since this C subset rejects an `extern` declaration and its
// matching definition inside the same file (which msdos.c including
// msdos.h would otherwise be), and a plain non-`extern` array in this
// header would instead give every file that includes it its own separate
// copy. Read the bytes dos_read() placed here with dos_buf_get(), or fill
// it with dos_buf_set() before dos_write().
unsigned char dos_buf_get(unsigned char i);
void dos_buf_set(unsigned char i, unsigned char value);

// Creates (and truncates if it exists) `path` with attribute byte `attrs`
// (0 = normal) via INT 21h/3Ch. `path` is staged through a 127-character
// buffer - longer paths are truncated, not rejected.
int dos_create(char *path, int attrs);
// Opens `path` via INT 21h/3Dh; `mode` is 0 (read), 1 (write), or 2
// (read/write). Same 127-character path limit as dos_create().
int dos_open(char *path, unsigned char mode);
// Closes `handle` via INT 21h/3Eh.
int dos_close(int handle);
// Reads up to `count` bytes (at most 255 - this wrapper makes one INT 21h
// call, no internal looping for more) from `handle` into the shared
// buffer dos_buf_get() reads, via INT 21h/3Fh. Returns the number of
// bytes actually read (0 at EOF).
int dos_read(int handle, unsigned char count);
// Writes the first `count` bytes (at most 255) of the shared buffer
// dos_buf_set() fills to `handle`, via INT 21h/40h. Returns the number of
// bytes actually written.
int dos_write(int handle, unsigned char count);
// Deletes `path` via INT 21h/41h.
int dos_delete(char *path);
// Renames `oldpath` to `newpath` via INT 21h/56h (same drive/directory
// rules as real DOS `REN`). Same 127-character path limit as dos_create().
int dos_rename(char *oldpath, char *newpath);
// Moves `handle`'s file pointer via INT 21h/42h: `method` is 0 (from the
// start), 1 (from the current position), or 2 (from the end). `offset` is
// a plain 16-bit `int` (this wrapper always sends a zero high word), so it
// only reaches the first 64KB of a file. Returns the resulting position's
// low 16 bits.
int dos_seek(int handle, int offset, unsigned char method);

// The current date/time via INT 21h/2Ah and 2Ch. Each getter makes its
// own full INT 21h call rather than caching one shared reading, so a
// torn read across two calls (e.g. `dos_date_day()` then
// `dos_date_month()` crossing midnight) is possible in principle, same as
// calling real DOS's own date/time commands twice in a row.
int dos_date_year();           // 1980-2099
unsigned char dos_date_month();     // 1-12
unsigned char dos_date_day();       // 1-31
unsigned char dos_date_weekday();   // 0 (Sunday) - 6 (Saturday)
unsigned char dos_time_hour();      // 0-23
unsigned char dos_time_minute();    // 0-59
unsigned char dos_time_second();    // 0-59
unsigned char dos_time_hundredths(); // 0-99

// The current command tail, read directly out of the PSP (bytes 0x80/
// 0x81+, absolute - a .com program's own DS already is the PSP segment,
// so no segment override is needed): dos_cmdline_len() is the tail's
// length in bytes (DOS's own convention: the leading space before the
// first argument counts, the trailing CR doesn't), dos_cmdline_char(i)
// is byte `i` of it (0-indexed).
unsigned char dos_cmdline_len();
unsigned char dos_cmdline_char(unsigned char i);

#endif
