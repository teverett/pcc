# 6502-vic20

A "hello world" for the Commodore VIC-20, compiled and linked against
`src/rt/vic20/` (see that directory's own `vic20.h`/`vic20.c`): clears
the screen and prints `HELLO, WORLD!` via the KERNAL's `CHROUT` ($FFD2)
call, the same way any real VIC-20 program does.

`make` runs the three-file pipeline: `main.c` compiles on its own
(`--platform=6502-prg`, which assumes `coff` since no `--link` is given
yet - see `docs/using.md`'s own `--platform` entry),
`src/rt/vic20/vic20.lib` is built (or reused) via that directory's own
Makefile, then the two combine via `--link --platform=6502-prg
--base=4097` into `build/main.prg` - `--base=4097` ($1001) is the
unexpanded VIC-20's own BASIC-program start address, where the KERNAL's
`LOAD` places (and, for a `prg` file whose own header matches, autoruns)
a loaded program; the default `$0600` base instead sits inside the
VIC-20's own zero-page/low-memory KERNAL work area, not this machine's
program area at all. The real PRG convention has no length field of its
own - just the 2-byte load address, then the linked bytes verbatim; see
`../../src/fileformat/prg/PrgFormat.h`'s own header comment. Getting
`main.prg` onto a real/emulated disk or tape still needs an existing
disk/tape-image tool (e.g. VICE's `c1541`) to inject it as a directory
or tape entry - or it can just be loaded directly by any VIC-20
emulator that reads a bare `.prg` file (VICE: `x64sc -autostart
main.prg`, or `xvic` for the real VIC-20 profile).

Since there's no VIC-20 KERNAL ROM in this project's own CI (`py65`'s
`MPU` is just the bare 6502 core, no firmware loaded), CI instead links
the same program as `build/main.raw` (`make build/main.raw`, identical
bytes minus the `prg` header) and patches a tiny hand-written 6502 stub
into the simulator's memory at `CHROUT`'s real address before running
it, so `main()`'s real `asm { jsr ... }` call lands somewhere that
actually records what was printed instead of crashing into empty
memory - see `test.py`'s own header comment for exactly what the stub
does.
