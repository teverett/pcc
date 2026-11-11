// Real disk I/O, one shared test, two completely different real
// hardware mechanisms selected by `#ifdef __PDP7__` / `#ifdef
// __8086__` (see ../docs/language.md's "Preprocessor" and
// "Inline asm") - not run by tests/run_tests.sh's normal `// EXPECT:`
// loop (there's no `// EXPECT:` comment here on purpose: a real
// round trip needs a disk device actually attached to the simulator,
// which that harness doesn't set up - see this file's own header for
// exactly how each side was verified by hand instead).
//
// Both halves do the same thing: write a known pattern to one disk
// sector, read it back into a different buffer, and return 1 if
// every word round-tripped correctly (0 otherwise) - the strongest
// check available short of inspecting the backing image file's own
// bytes directly (also done, see below).
//
// Verified on the real `pdp7` simulator: `set rb ena` / `att rb
// <scratch file>` (same as `../../src-assem/build/unixv0.simh`'s own
// real boot), `load`/`go`, then `examine ac` (1 = success) and
// `examine rb sta`/`da`/`wc`/`ma` to directly confirm the real RB09
// controller's own registers after the run - `wc` reaching exactly 0
// and `da` having advanced by exactly 64 words (one full sector)
// confirmed a genuine 64-word transfer actually happened, not just a
// lucky-looking halt. The backing file's own bytes were inspected
// too (SIMH's `rb` stores each 18-bit word as 4 bytes): the pattern
// landed at byte offset 512 - exactly sector 2's own start (128
// words in) times 4 bytes/word, confirming the write hit the right
// physical location, not just *some* location.

#ifdef __PDP7__
// PDP-7 side: the RB09 fixed-head disk's own raw IOT instructions
// (../src-assem/sys/sop.s's own opcode-literal definitions - `dscs`/
// `dslw`/`dslm`/`dsld`/`dsls`/`dssf`/`dsrs`, see
// ../docs/language.md's "Inline asm"), each wrapped by this
// language's existing `dac`-leaves-AC-unchanged invariant (the same
// one `x = y = 5` already relies on - see ../docs/language.md's
// "The pdp7 backend"): a bare expression-statement naming a local
// (e.g. `wordCount;`) loads that value into AC via an ordinary `lac`,
// with nothing to `dac` it back to - so AC holds exactly that value
// going into the very next `asm { ... }` block, which is how a raw
// IOT instruction that "loads from AC" gets the right operand
// without rcc understanding what's inside the block at all.
//
// This follows ../src-assem/sys/s4.s's own `dsktrans` protocol
// exactly for the parts that are unambiguous from its source
// (dscs -> dslw(wordCount) -> dslm(bufAddr) -> dsld(bcdTrackSector)
// -> dsls(opCode), word count -64 for one real 64-word sector,
// 3072/3584 decimal - octal 06000/03000... see below - for read/
// write, matching the real kernel's own literal constants exactly)
// but *deliberately does not* replicate `dskio`'s own general
// logical-block-number -> BCD-track/sector conversion, which uses
// EAE instructions (`idiv`/`lls`/`als`) whose exact bit semantics
// aren't independently verified anywhere in this project (see
// ../../src-c/README.md's own note on `__divmod` avoiding `idiv` for
// the same reason). Sidestepped entirely by calling this with
// `track=0` and a single-digit `sector` directly: BCD encodes each
// decimal digit in its own nibble, so BCD and plain binary are
// *identical* for any value 0-9, and track 0 contributes no bits at
// all regardless of where the real hardware's word format would
// otherwise put it - so the exact track/sector packing convention
// genuinely doesn't matter here, only for a real multi-track address.
//
// Op-code words: this project's own language has no octal literals
// (see ../README.md's "Scope and limitations") - 3072/3584 decimal
// are octal 06000/07000 (6*512 / 7*512), the exact read/write op
// words `../src-assem/sys/s4.s`'s own `jms dskio; 06000`/`;07000`
// call sites use.
//
// Deliberately NOT replicated: the real kernel's interrupt-driven
// completion wait (`.dskb`/`.dske`, needing a real interrupt vector
// table this project doesn't set up - see ../../src-c/README.md's
// "What compiling the kernel needs"). This busy-polls `dssf` directly
// instead ("skip if disk flag set" - skip-on-done, same DEC IOT
// naming convention as the paper-tape reader's `rsf`/`rsa` already
// used in `../src-assem/sys/s9.s`'s `getc`) - a real, working, but
// intentionally different mechanism from the original's own, same
// "behavioral equivalence, not byte-identical" spirit as every other
// port in this project.
// The seven raw IOT mnemonics (`dscs`/`dslw`/`dslm`/`dsld`/`dsls`/
// `dssf`/`dsrs`) aren't in `Pdp7Assembler`'s builtin opcode table
// (they're hardware disk-controller instructions, not ordinary CPU
// ones - see this project's own in-process assembler, which copies
// its table verbatim from `../../pdp-assembler`'s, which never had
// them either) - so, same as `_ion()`/`_iof()`'s own backend codegen,
// each has to be written here as its literal octal instruction word
// (`../src-assem/sys/sop.s`'s own definitions), not its mnemonic.
int dskio_lowlevel(int wordCount, int bufAddr, int bcdTrackSector, int opCode) {
    asm { 0707141 }
    wordCount;
    asm { 0707124 }
    bufAddr;
    asm { 0707142 }
    bcdTrackSector;
    asm { 0707104 }
    opCode;
    asm { 0707144 }
    asm {
RCCDSKWAIT:
        0707121
        jmp RCCDSKWAIT
        0707132
    }
    return 0;
}

int dskRead(int sector, int bufAddr) {
    return dskio_lowlevel(-64, bufAddr, sector, 3072);
}

int dskWrite(int sector, int bufAddr) {
    return dskio_lowlevel(-64, bufAddr, sector, 3584);
}

int writeBuf[64];
int readBuf[64];

int main() {
    int i;
    int ok;
    for (i = 0; i < 64; i = i + 1) {
        writeBuf[i] = i + 100;
        readBuf[i] = 0;
    }
    dskWrite(2, &writeBuf[0]);
    dskRead(2, &readBuf[0]);
    ok = 1;
    for (i = 0; i < 64; i = i + 1) {
        if (readBuf[i] != i + 100) {
            ok = 0;
        }
    }
    return ok;
}
#endif

#ifdef __8086__
// 8086 side: real BIOS disk I/O (`int 0x13`), the actual way a real
// boot-sector program reads/writes sectors on real hardware (and
// under qemu-system-i386's own BIOS) - no equivalent of pdp7's "AC
// stays put after a store" trick exists here (`int 0x13` needs
// AH/AL/CH/CL/DH/DL/ES:BX all set *simultaneously*, not one register
// fed by consecutive C expression-statements), so this whole round
// trip is one `asm { ... }` block with literal operands rather than
// pdp7's small-asm-blocks-plus-C-parameters style - the natural shape
// for this side regardless of language, since real NASM code calling
// `int 0x13` is normally written exactly this way too.
//
// CHS addressing: cylinder 0, head 0, sector 2 (BIOS sector numbers
// are 1-based - sector 1 is this program's own boot sector, so
// sector 2 is the next one on the disk), drive 0 (the floppy this
// image itself boots from - `qemu-system-i386 -fda <this binary>`).
// Scratch buffer at 0x1000: comfortably below this backend's own
// `ORG 0x7C00` (where code/data starts and grows upward) and below
// where the stack (also based at 0x7C00, growing downward) could
// reach for a program this small - see
// ../docs/language.md's "The 8086 backend".
//
// `return;` with no expression, same trick as pdp7's AC-retention
// but for `ax`: I8086Backend's own `Return` codegen only emits `mov
// ax, <expr>` when a `return` actually has one - a bare `return;`
// just emits `ret`, leaving whatever this asm block already put in
// `ax` (1 success / 0 failure) as the real result the caller reads.
//
// Verified under the real `qemu-system-i386` (`info registers` over
// the monitor, same as `tests/run_tests.sh`'s own `check_8086`) and
// by inspecting the booted floppy image's own bytes afterward - the
// pattern landed at byte offset 512 (sector 2, 1-based, 512
// bytes/sector), confirming a real write to the right physical
// sector, not just a lucky-looking success flag. **The backing
// `.bin` file has to be padded to a real floppy's full size (1.44MB,
// 1474560 bytes) before attaching it to qemu** - BIOS's own `int
// 0x13` rejects a too-small image outright (`AH=0x20`, "controller
// failure", confirmed directly by checking `AH` after a failing call)
// since it can't recognize a bare 512-byte file as any known floppy
// geometry; this project's own `--format=bin` output is deliberately
// still just the bare, unpadded boot sector (see
// ../docs/language.md's "The 8086 backend") - padding for a
// real multi-sector disk test is this test's own verification-step
// concern, not something `rcc` itself needs to do.
//
// `I8086Assembler` is "not a general x86 encoder: exactly the
// mnemonic/operand shapes `I8086Backend.cpp` ever emits, nothing
// else" (see I8086Assembler.cpp's own header comment) - three real
// gaps had to be closed/worked around to write this:
// - It has no `loop` - each loop below is spelled out as `add cx,
//   -1` / `cmp cx, 0` / `jne`, shapes it already supported.
// - `cmp` only ever takes two register operands or a register and an
//   immediate, never a memory operand - VERIFYLOOP loads through
//   memory into `dx` first (`mov dx, [bx]`) rather than comparing a
//   memory operand directly.
// - Its only register-indirect addressing form is `[bx]` specifically
//   (see I8086Assembler.cpp's own `Operand`/`buildOperand` comments -
//   `[label]`, `[bx]`, `[label+bx]` are the only three shapes this
//   backend's codegen ever needs) - `bx` is reused as the one pointer
//   register for every loop below *and* as `int 0x13`'s own buffer-
//   offset register, since they're genuinely the same address each
//   time, not a workaround.
// `int imm8` (the real BIOS/DOS software-interrupt instruction - see
// `encodeInstr`'s own comment there) is new too: nothing else this
// backend ever generates needed it before this file. One more real
// gap: no 8-bit sub-registers (`ah`/`al`/`ch`/`cl`/`dh`/`dl`) either
// - this language is 16-bit-int-only, so the backend never had a
// reason to support them. `int 0x13` needs each pair set
// independently (AH=function, AL=count; CH=cylinder, CL=sector;
// DH=head, DL=drive), so each pair is loaded as one combined 16-bit
// value instead (`ax = (ah<<8)|al`, etc.) - e.g. `769` below is
// `(3<<8)|1` (AH=3 "write", AL=1 "one sector"), `513` is `(2<<8)|1`
// ("read", one sector), `2` is `(0<<8)|2` (cylinder 0, sector 2), and
// plain `0` covers DH=DL=0 (head 0, drive 0 - this program's own
// floppy).
int main() {
    asm {
        mov bx, 0x1000
        mov cx, 64
        mov ax, 100
FILLLOOP:
        mov [bx], ax
        add ax, 1
        add bx, 2
        add cx, -1
        cmp cx, 0
        jne FILLLOOP

        xor ax, ax
        mov es, ax
        mov bx, 0x1000
        mov ax, 769
        mov cx, 2
        mov dx, 0
        int 0x13

        mov bx, 0x1000
        mov cx, 64
        mov ax, 0
CLEARLOOP:
        mov [bx], ax
        add bx, 2
        add cx, -1
        cmp cx, 0
        jne CLEARLOOP

        mov bx, 0x1000
        mov ax, 513
        mov cx, 2
        mov dx, 0
        int 0x13

        mov bx, 0x1000
        mov cx, 64
        mov ax, 100
VERIFYLOOP:
        mov dx, [bx]
        cmp dx, ax
        jne DISKFAIL
        add ax, 1
        add bx, 2
        add cx, -1
        cmp cx, 0
        jne VERIFYLOOP
        mov ax, 1
        jmp DISKDONE
DISKFAIL:
        mov ax, 0
DISKDONE:
    }
    return;
}
#endif
