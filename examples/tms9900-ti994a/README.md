# tms9900-ti994a

A "hello world" for the TI-99/4A, built against `--target=tms9900` (see
`docs/platforms.md`'s own `tms9900` entry for the full backend
description - a genuinely different architecture from every other
target in this project, with 16 "registers" that are just RAM words
addressed through a Workspace Pointer, not real CPU registers).

Initializes the TMS9918A VDP into Graphics Mode I, clears the screen,
then prints `HELLO, WORLD!` at row 10, column 9, via `src/rt/ti994a/` -
a runtime library that reaches the VDP as plain memory addresses
(`$8800`/`$8802`/`$8C00`/`$8C02`), not CRU bits or Z80-style ports, the
only hardware simple enough to drive without the console's own GPL
interpreter/ROM. See `src/rt/ti994a/ti994a.h`'s own header comment for
why `ti_vdp_init_text_mode()` has to be called explicitly first here,
unlike `../z80-coleco/`'s closely related VDP: a program here is
`rcc`'s own `_start`, never the console's real cartridge-boot sequence,
so nothing guarantees the VDP starts in any particular mode.

## Building

```
make                # produces build/main.raw
```

Assumes the real 32K memory expansion is present (the console's own
low 8K at `$2000`-`$3FFF` and high 24K at `$A000`-`$FFFF`) - without it,
the stock TI-99/4A has only a 256-byte scratchpad for general RAM, far
too little to run a linked C program at all; this was nearly universal
on real TI-99/4A software too. `--format=raw` is the only output this
target gets - no native cartridge/disk container format is implemented
(out of scope for this pass).

## Why no real hardware/emulator to test against

No pip-installable Python TMS9900 core exists (see `docs/known-bugs.md`
for exactly how this backend was verified anyway, including the VDP
write sequences this very runtime emits). `make test` here only checks
that `build/main.raw` exists, the same convention
`../m30626fjpfp-freestanding/`, `../8086-msdos/`, and `../6809-coco/`
use for a target with no simulator wired into this project at all.
