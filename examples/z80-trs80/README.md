# z80-trs80

A "hello world" for the TRS-80 Model I/III, compiled and linked against
`src/rt/trs80/` (see that directory's own `trs80.h`/`trs80.c`): clears the
64x16 text screen and prints `HELLO, WORLD!` centered on row 7, via direct
stores into video RAM (`$3C00`-`$3FFF`) rather than any ROM/DOS call.

`make` runs the three-file pipeline: `main.c` compiles on its own
(`--format=coff`), `src/rt/trs80/trs80.lib` is built (or reused) via that
directory's own Makefile, then the two combine via `--link --format=cmd`
into `build/main.cmd` - the real TRS-80 DOS "/CMD" load-module byte stream
(TYPE/LENGTH/DATA records; see `../../src/fileformat/cmd/CmdFormat.h`'s own
header comment for exactly what each record means) that TRSDOS/LDOS/NEWDOS
load and run directly off a real/emulated floppy, with no translation step
needed.

Since there's no TRS-80 emulator in this project's own CI, CI instead links
the same program as `build/main.raw` (`make build/main.raw`, identical
`.text`/entry bytes minus `cmd`'s own record wrapping) and runs it on a
real, independent Z80 core (`z80-python`), then reads the expected text
straight back out of simulated video RAM - no ROM to fake at all, since
`trs80.c` never makes a ROM/DOS call in the first place.
