# 6502-c64

A "hello world" for the Commodore 64, compiled and linked against
`src/rt/c64/` (see that directory's own `c64.h`/`c64.c`): clears the
screen and prints `HELLO, WORLD!` via the KERNAL's `CHROUT` ($FFD2) call
- the same KERNAL jump-table address `examples/6502-vic20/` uses, since
both machines share the identical Commodore 8-bit KERNAL lineage (see
`src/rt/c64/c64.h`'s own header comment for exactly what's duplicated
and why it isn't shared between the two directories).

`make` runs the three-file pipeline: `main.c` compiles on its own
(`--platform=6502-prg`, which assumes `coff` since no `--link` is given
yet - see `docs/using.md`'s own `--platform` entry),
`src/rt/c64/c64.lib` is built (or reused) via that directory's own
Makefile, then the two combine via `--link --platform=6502-prg
--base=2049` into `build/main.prg` - `--base=2049` ($0801) is the C64's
own BASIC-program start address, where the KERNAL's `LOAD` places (and,
for a `prg` file whose own header matches, autoruns) a loaded program;
the default `$0600` base instead sits inside the C64's own zero-page/
low-memory KERNAL work area, not this machine's program area at all.
The real PRG convention has no length field of its own - just the
2-byte load address, then the linked bytes verbatim; see
`../../src/fileformat/prg/PrgFormat.h`'s own header comment. Getting
`main.prg` onto a real/emulated disk or tape still needs an existing
disk/tape-image tool (e.g. VICE's `c1541`) to inject it as a directory
or tape entry - or it can just be loaded directly by any C64 emulator
that reads a bare `.prg` file (VICE: `x64sc -autostart main.prg`).

Since there's no C64 KERNAL ROM in this project's own CI (`py65`'s
`MPU` is just the bare 6502 core, no firmware loaded), CI instead links
the same program as `build/main.raw` (`make build/main.raw`, identical
bytes minus the `prg` header) and patches a tiny hand-written 6502 stub
into the simulator's memory at `CHROUT`'s real address before running
it, so `main()`'s real `asm { jsr ... }` call lands somewhere that
actually records what was printed instead of crashing into empty
memory - see `test.py`'s own header comment for exactly what the stub
does.
