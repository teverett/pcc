# z80-zxspectrum

A "hello world" for the 48K ZX Spectrum, built against the same `--target=z80`
every other z80 platform here uses (see `docs/platforms.md`'s own
`zx spectrum` entry for why there's no separate target - unlike
ColecoVision's cartridge-header entry stub or its 1KB RAM ceiling, the
Spectrum has a full, uncontended 48KB RAM window and no memory-map
hazard to work around).

Unlike `../z80-coleco/` or `../z80-trs80/` (both print real text), this
sets the border color, fills the screen's attribute memory with a solid
ink/paper combination, and plots an "X" shape directly into screen
memory - every one of those is real ULA/memory-mapped hardware, not
ROM-mediated: a real `.sna` snapshot boots straight into user code via a
bootstrap trick (see `../../src/fileformat/sna/SnaFormat.h`'s own header
comment) rather than running the ROM's own reset sequence first, so
there's no real, correctly-initialized system-variable state the ROM's
print routine (`RST $16`) could rely on without this project faking a
byte-correct copy of it - see `../../src/rt/zxspectrum/zxspectrum.h`'s
own header comment for exactly what that runtime library covers (border/
speaker, keyboard, attributes, and the screen bitmap) and what it
deliberately doesn't (anything ROM-dependent, or needing precisely
timed loops - beeper tones, tape I/O).

## Building

```
make                       # produces build/main.sna, a real 48K snapshot
make build/main.raw        # the same program, unwrapped (no snapshot header)
```

`--platform=z80-sna` (used for both the compile step, where it assumes
`coff` since no `--link` is given yet, and the `--link` step itself -
see `docs/using.md`'s own `--platform` entry) links the same way every
other `z80` program does; `--format=sna` just wraps the result as a
27-byte register header plus a 49152-byte 48K RAM dump, with the
program's own bytes placed at the right offset and the 2 bytes at
`$FFFE`-`$FFFF` reserved for the bootstrap entry address.

That `.sna` file loads directly in any Spectrum emulator that reads one
(Fuse, ZEsarUX, ZXSpin) - no tape-loading step, no BASIC loader.

## Why no real hardware/ROM to test against

Like `../z80-coleco/`'s ColecoVision BIOS, there's no ZX Spectrum ROM
bundled with this project. CI instead links `build/main.raw` (the exact
same bytes `main.sna` wraps) and runs it on an independent Z80 core
(`z80-python`), checking the one real `OUT $FE` instruction
`zx_set_border()` emits, and reading the simulator's own memory array
directly for the attribute fill and the plotted pixels (both ordinary
memory stores, not ports) - see `test.py`'s own header comment.
`tests/run_zxspectrum_tests.sh` goes further, separately exercising
every other function in `src/rt/zxspectrum/` (speaker, keyboard,
individual attribute/pixel writes) the same way.
