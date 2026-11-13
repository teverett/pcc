# 68000-freestanding

Same `main.c` as m30626fjpfp-freestanding/6502-freestanding, but compiled and
linked as a freestanding (bare-metal) binary for the `68000` target instead,
producing a flat `main.bin` with no OS or C library.

`make` runs the two-step compile then link pipeline used for freestanding
targets, both steps as `--platform=68000-raw` - the compile step assumes
`coff` automatically since no `--link` is given yet (see `docs/using.md`'s
own `--platform` entry).

Since `main.bin` has no OS to run it on, CI verifies it with
`qemu-system-m68k -M none -cpu m68000` instead: a bare CPU with no boot
ROM/vector table, loaded at `0x1000` (the `--format=raw` linker's load
address) with QEMU's `loader` device forcing the CPU's PC there directly
(bypassing the real 68000 reset exception's own SSP/PC-from-vector-table
dance), then run until `main()`'s return value can be read back out of
register `D0` (see `tests/run_tests.sh`'s `check_68000()`).
