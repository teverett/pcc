# 8086-msdos

A "hello world" for MS-DOS, compiled and linked against `src/rt/msdos/`
(see that directory's own `msdos.h`/`msdos.c`): prints `HELLO, WORLD!` via
`INT 21h` function `09h` (`dos_print()`), the same way any real DOS
program does.

`make` runs the two-file pipeline: `main.c` compiles on its own
(`--target=8086-dos --format=coff`), `src/rt/msdos/msdos.lib` is built (or
reused) via that directory's own Makefile, then the two combine via
`--link --format=raw` into `build/main.com` - DOS's own `.com` convention
has no header of its own, so `--format=raw`'s plain bytes, loaded/entered
at `0x100` (see `docs/platforms.md`'s own `8086-dos` entry), are already a
real, directly-runnable `.com` file. Run it under DOSBox, a `dosemu`
install, or real MS-DOS (`main.com` at the DOS prompt); there's no CI
coverage for it yet since no DOS/8086 emulator is wired into
`tests/run_tests.sh` the way `py65` is for `6502`/`65c02`.
