# 6502-atari400

A "hello world" for the Atari 400/800, compiled and linked against
`src/rt/atari400/` (see that directory's own `atari400.h`/`atari400.c`):
clears the screen and prints `HELLO, WORLD!` via the Atari OS's CIO
handler (`CIOV`, `$E456`), the same way any real Atari program does.

Unlike `examples/6502-vic20/`'s and `examples/6502-c64/`'s KERNAL calls
(every one of which only ever touches the accumulator), CIO needs the
IOCB number in the X register - the one real new wrinkle this target
needed: rcc's 6502/65c02 assemblers had no X-register support at all
until `ldxi` (immediate-only `LDX`) was added specifically for this (see
`src/codegen/m6502/M6502Assembler.cpp`'s own `encodeInstr()` comment).
Every `jsr $E456` here still only ever sets X to the constant `0`
(IOCB 0, the screen editor `"E:"`, already open by the OS at boot) - no
general X-register support (no `STX`/`TAX`/indexed addressing) was
added.

`make` runs the three-file pipeline: `main.c` compiles on its own
(`--platform=6502-atari`, which assumes `coff` since no `--link` is
given yet - see `docs/using.md`'s own `--platform` entry),
`src/rt/atari400/atari400.lib` is built (or reused) via that directory's
own Makefile, then the two combine via `--link --platform=6502-atari
--base=8192` into `build/main.atari` - `--base=8192` ($2000) is clear
of both the frame-storage region `M6502Codegen.cpp` reserves at
`$0200-$05FF` and the OS's own low-memory work area, a reasonable load
address for a cartridge-free machine with at least 16K of RAM (see
`src/rt/atari400/atari400.h`'s own header comment for the caveat that
this hasn't been checked against every real memory configuration). The
real Atari DOS binary-load convention is a sequence of segments, each
its own start/end address plus data - no single fixed header the way
`PrgFormat`'s is; see `../../src/fileformat/atari/AtariExeFormat.h`'s
own header comment for exactly what's in each segment, including the
second, `RUNAD`-poking segment this writer appends so the output file
is directly autorunnable (as `AUTORUN.SYS`) with no separate boot-loader
stub. Getting `main.atari` onto a real/emulated disk still needs an
existing disk-image tool to inject it as a directory entry - or it can
just be loaded directly by any Atari emulator that reads a bare
executable file (Altirra: drag-and-drop, or boot it as `AUTORUN.SYS` on
a disk image).

Since there's no Atari OS ROM in this project's own CI (`py65`'s `MPU`
is just the bare 6502 core, no firmware loaded), CI instead links the
same program as `build/main.raw` (`make build/main.raw`, identical
program bytes minus the segment/`RUNAD` framing) and patches a tiny
hand-written 6502 stub into the simulator's memory at `CIOV`'s real
address before running it, so `main()`'s real `asm { jsr ... }`/`ldxi`
calls land somewhere that actually records what was printed instead of
crashing into empty memory - see `test.py`'s own header comment for
exactly what the stub does.
