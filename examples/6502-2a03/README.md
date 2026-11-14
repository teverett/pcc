# 6502-2a03

A "hello world" for the NES's 2A03, built against `--target=2a03` (see
`docs/platforms.md`'s own `2a03` entry and `src/codegen/m6502/
M6502Codegen.h`'s header comment for why it's a distinct target rather
than just `--target=6502 --format=raw`).

Unlike the Atari 2600 (`examples/6502-atari2600/`), the 2A03 has no
memory-map hazard to work around - it's a real, uncontended 2KB RAM at
$0000-$07FF, the same zero-page/frame-storage layout every other 6502
platform here already assumes. But it still has no runtime library:
real NES graphics/sound need the PPU/APU driven cycle-by-cycle in sync
with the video signal, which no C compiler's ordinary codegen can
promise (the same reasoning as the 2600's TIA). So this example goes one
step further than the 2600's solid-color poke, since there's real RAM
to exercise: `main.c` sums `1..10` in an ordinary C `for` loop, then
pokes the result into a fixed zero-page scratch cell ($0050, `asm { sta
80 }`) so a test can read it back without needing to know where the
linker placed the `result` global itself.

`make` links `main.c` straight into `build/main.nes`, a real iNES ROM
image (`--platform=2a03-nes --base='$8000'` - NROM's own fixed PRG-ROM
window, the only base `--format=nes` accepts; see `../../src/
fileformat/nes/NesRomFormat.h`). That file loads directly in any NES
emulator that reads a `.nes` file (e.g. Mesen, FCEUX).

Since no PPU/APU/mapper hardware is simulated in this project's own CI
(`py65`'s `MPU` is just the bare 6502 core), CI instead links the same
program as `build/main.raw` (`make build/main.raw`, identical bytes
minus the iNES header/padding/vector table) and just checks that $0050
holds 55 after the program reaches its own halt loop - see `test.py`'s
own header comment.
