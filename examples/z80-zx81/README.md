# z80-zx81

A minimal demo for the ZX81, built against plain `--target=z80` (see
`docs/platforms.md`'s own `zx81` entry for why there's no separate
target or native file format for this platform).

Unlike `../z80-cpc/` or `../z80-zxspectrum/`, there's no border/bitmap/
sound primitive to demonstrate here: the ZX81 has none of that hardware
at all - see `../../src/rt/zx81/zx81.h`'s own header comment. This
example only enables the display's NMI generator, reads one keyboard
half-row, then disables the display generator again - the only two
pieces of ZX81 hardware simple enough to reach without a ROM call, and
even these carry a real confidence caveat the other z80 platforms here
don't: every source consulted while writing `src/rt/zx81/` agreed on
*what* port `$FE`/`$FD` do, but also warned that keyboard reading from
machine code is unreliable outside `SLOW` mode, with no official
technical reference or ZX81 emulator available in this project to
verify either claim against.

## Building

```
make                # produces build/main.raw
```

Just `--format=raw` - see `../../src/rt/zx81/zx81.h`'s own header
comment for why this platform gets no AMSDOS-/`.sna`-style native
container: the ZX81's own `.p` tape format is tied to resuming a saved
*BASIC program's* own continuation state, not a general CPU-state
snapshot, and no source consulted agreed closely enough on its exact
header/field layout to implement with any confidence.

## Why no real hardware/ROM to test against

Like `../z80-coleco/`'s ColecoVision BIOS, there's no ZX81 ROM bundled
with this project - and unlike that case, there's no independent
emulator here either to cross-check the keyboard-read sequence against.
CI instead runs `build/main.raw` on an independent Z80 core
(`z80-python`) and checks only that the code issues the exact port
sequence `zx81.h` documents (`OUT $FE`, `IN A,(n)` with the row-select
byte on the upper address bus, `OUT $FD`) - see `test.py`'s own header
comment for exactly what that does and doesn't prove.
