# z80-cpc

A "hello world" for the Amstrad CPC (464/664/6128, 64K), built against the
same `--target=z80` every other z80 platform here uses (see
`docs/platforms.md`'s own `amstrad cpc` entry for why there's no
separate target - the CPC's free RAM window comfortably contains plain
`z80`'s own default link base, `$8000`).

Sets screen mode 2 (640x200 monochrome), two ink colors and the border
via the Gate Array (`src/rt/cpc/cpc.h`'s `cpc_set_mode()`/`cpc_set_pen()`
- two real `OUT`s to port `$7F` each), clears the screen, then plots an
"X" shape directly into screen memory. Every one of those is real,
documented CPC hardware, no ROM/firmware call - see `src/rt/cpc/cpc.h`'s
own header comment for exactly what this runtime covers and what it
deliberately doesn't (the keyboard, modes 0/1's pixel packing, disk
access), including a real hazard discovered while building it: this
backend's Z80 hardware stack starts at a fixed `$FF00` shared by every
z80 platform, which collides with the very top of the CPC's own screen
RAM (`$C000`-`$FFFF`) - see `docs/known-bugs.md`'s own entry on this.
`cpc_cls()`/`cpc_set_pixel()` stay clear of it by leaving the last 512
bytes of screen memory (and the bottom ~48 of 200 scanlines) untouched.

## Building

```
make                       # produces build/main.bin, a real AMSDOS binary file
make build/main.raw        # the same program, unwrapped (no AMSDOS header)
```

`--platform=z80-amsdos` links the same way every other `z80` program
does; `--format=amsdos` just wraps the result in the real 128-byte
AMSDOS header every CPC disk/tape binary carries (see `../../src/
fileformat/amsdos/AmsdosFormat.h`) - the convention AMSDOS's own `LOAD`/
`RUN` and most CPC emulators read to find the program's own load/entry
address. Getting `build/main.bin` onto a real/emulated disk still needs
an external disk-image tool to inject it as a catalog entry, the same
caveat `../z80-trs80/`'s `.CMD` file and `../6809-coco/`'s `.decb` file
both have.

## Why no real hardware/ROM to test against

Like `../z80-coleco/`'s ColecoVision BIOS, there's no Amstrad CPC
firmware bundled with this project. CI instead links `build/main.raw`
(the exact same bytes `main.bin` wraps) and runs it on an independent
Z80 core (`z80-python`), checking the real Gate Array `OUT`s
`cpc_set_mode()`/`cpc_set_pen()` emit, and reading the simulator's own
memory array directly for the plotted pixels (an ordinary memory
store, not a port) - see `test.py`'s own header comment.
`tests/run_cpc_rt_tests.sh` goes further, separately exercising every
other function in `src/rt/cpc/` (the PSG write sequence, individual
pixel writes) the same way.
