# 6502-dos33

A "hello world" for the Apple ][, compiled and linked against
`src/rt/apple2/` (see that directory's own `apple2.h`/`apple2.c`): sets
40-column text mode and tabs to row 10 (`SETTXT`/`VTAB`), then prints
`HELLO, WORLD!` in inverse video (`SETINV`/`SETNORM`) via the monitor
ROM's `COUT`/`CROUT` calls, the same way any real Apple ][ program does.

`make` runs the three-file pipeline: `main.c` compiles on its own
(`--format=coff`), `src/rt/apple2/apple2.lib` is built (or reused) via
that directory's own Makefile, then the two combine via `--link
--format=prodos --base=8192` into `build/main.prodos` - `--base=8192`
($2000) keeps the linked program clear of the Apple ]['s own text page 1
(`$0400`-`$07FF`), which the default `$0600` base sits inside of and
which this program's own `COUT`/`CROUT` calls write through; $2000 is
also where ProDOS's own loader always enters a `SYS` file regardless of
its directory entry's `aux_type` (see `src/linker/factory/
M6502LinkerFactory.h`'s own comment). The real DOS 3.3/ProDOS "B
file" convention has no
header/footer of its own filesystem bookkeeping baked in; see
`../../src/fileformat/prodos/ProDosFormat.h`'s own header comment for
exactly what the output's trailing 3 bytes (`file_type`/`aux_type`) mean.
Getting `main.prodos` onto a real/emulated disk still needs an existing
disk-image tool (e.g. AppleCommander) to inject it as a ProDOS directory
entry, using those two trailing bytes as that tool's own `-t`/`-a` flags
(after stripping them back off the file).

Since there's no Apple ][ ROM in this project's own CI (`py65`'s `MPU` is
just the bare 6502 core, no firmware loaded), CI instead links the same
program as `build/main.raw` (`make build/main.raw`, identical bytes minus
the `prodos` footer) and patches tiny hand-written 6502 stubs into the
simulator's memory at `COUT`/`CROUT`/`HOME`/`SETTXT`/`VTAB`/`SETINV`/
`SETNORM`'s real addresses before running it, so `main()`'s real
`asm { jsr ... }` calls land somewhere that actually records what was
printed instead of crashing into empty memory - see
`.github/workflows/platformtests.yml`'s own comment for exactly what
each stub does.
