# 6809-coco

A "hello world" for the TRS-80 Color Computer (CoCo), compiled and linked
against `src/rt/coco/` (see that directory's own `coco.h`/`coco.c`): clears
the 32x16 text screen and prints `HELLO, WORLD!` centered on row 7, via
direct stores into video RAM (`$0400`-`$05FF`) rather than any ROM/BASIC
call.

`make` runs the three-file pipeline: `main.c` compiles on its own
(`--format=coff`), `src/rt/coco/coco.lib` is built (or reused) via that
directory's own Makefile, then the two combine via `--link` into both
`build/main.raw` (a flat binary, useful for feeding to any tool that
loads raw bytes at a fixed address) and `build/main.decb` (the real
CoCo `LOADM`/`CLOADM` load-module byte stream - see
`../../src/fileformat/decb/DecbFormat.h`), ready for a real CoCo's
`LOADM`/`EXEC` or `CLOADM`/`EXEC` from cassette.

There is no verified 6809 emulator in this project's CI yet, so CI only
confirms this example compiles and links (`make -C examples/6809-coco`) -
it doesn't execute either output against anything.
