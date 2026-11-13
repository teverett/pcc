# z80-coleco

A ColecoVision "hello world": clears the 32x24 text screen, then prints
`HELLO, WORLD!` at row 10, column 9, via `src/rt/coleco/` - a runtime
library that reaches the TMS9928A VDP, the controller ports, and the
SN76489 sound chip entirely through the Z80's `IN`/`OUT` instructions
(see `src/rt/coleco/coleco.h`'s own header comment, and
`src/codegen/z80/Z80Assembler.h`'s, for why those needed adding to this
backend's assembler at all - no codegen emits them, only this RT's own
`asm{}` blocks).

## Building

```
make                       # produces build/main.col, a padded cartridge image
make build/main.raw        # the same program, unpadded (no standard-size rounding)
```

`--platform=z80-coleco-colecorom` (used for both the compile step, where
it assumes `coff` since no `--link` is given yet, and the `--link` step
itself - see `docs/using.md`'s own `--platform` entry) compiles with the
same Z80 backend as `z80-*` targets (see `docs/platforms.md`) but links
against a ColecoVision-specific entry object: the cartridge header (a
`$55AA`/`$AA55` signature and pointer table the BIOS reads before ever
jumping to this program's own code - see
`src/linker/factory/ColecoLinkerFactory.cpp`'s own header comment)
instead of TRS-80's/8080's plain `call fn_main; halt`.

Its `colecorom` format pads the linked image with `$FF` up to the
nearest standard cartridge size (8/16/24/32 KiB - see
`src/fileformat/colecorom/ColecoRomFormat.h`); `--platform=z80-coleco-raw`
is the same bytes without that padding, used for this example's own CI
check (and by `tests/run_coleco_rt_tests.sh`) since a Z80 simulator
doesn't care how big the file around the bytes it actually executes is.

## Why no real hardware/BIOS to test against

Like `../z80-trs80/`, there's no ColecoVision BIOS dump bundled with this
project (and no official technical reference to confirm the cartridge
header's exact field layout or the VDP's default register state
byte-for-byte - see `coleco.h`'s own header comment). CI instead loads
`build/main.raw` into an independent Z80 core (`z80-python`) with a small
synthetic VDP/controller model standing in for the real chips - just
enough to observe the name-table bytes `coleco_puts()` writes through
real `OUT`/`IN` instructions, not a cycle-accurate TMS9928A.
