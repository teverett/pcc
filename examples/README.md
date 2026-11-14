# examples

Small, buildable programs demonstrating `rcc` usage, each in its own
directory with a `Makefile` and a `README.md` of its own:

- `hosted-hello` — hosted executable for the current desktop OS (macOS/Linux/Windows)
- `m30626fjpfp-freestanding` — freestanding binary for the `m30626fjpfp` target
- `6502-freestanding` — freestanding binary for the `6502` target
- `68000-freestanding` — freestanding binary for the `68000` target
- `pdp7-freestanding` — freestanding RIM-tape binary for the `pdp7` target
- `6502-2a03` — NES "hello world" for `2a03`, linked as
  `--platform=2a03-nes --base='$8000'` into an iNES ROM image
- `6502-atari2600` — Atari 2600 "hello world" for `atari2600`, linked as
  `--platform=atari2600-a26 --base='$F000'` into a cartridge image
- `6502-atari400` — Atari 400/800 "hello world" for `6502`, linked as
  `--platform=6502-atari` against `src/rt/atari400/`'s CIO-call runtime
  library
- `6502-c64` — Commodore 64 "hello world" for `6502`, linked as
  `--platform=6502-prg` against `src/rt/c64/`'s KERNAL-call runtime
  library
- `6502-dos33` — Apple ][ "hello world" for `6502`, linked as
  `--platform=6502-prodos` against `src/rt/apple2/`'s ROM-call runtime
  library
- `6502-vic20` — Commodore VIC-20 "hello world" for `6502`, linked as
  `--platform=6502-prg` against `src/rt/vic20/`'s KERNAL-call runtime
  library
- `8086-msdos` — MS-DOS "hello world" for `8086-dos`, linked as
  `--platform=8086-dos-raw` (a directly-runnable `.com`) against
  `src/rt/msdos/`'s `INT 21h` runtime library
- `z80-trs80` — TRS-80 "hello world" for `z80`, linked as
  `--platform=z80-cmd` against `src/rt/trs80/`'s video-RAM runtime
  library
- `z80-coleco` — ColecoVision "hello world" for `z80-coleco`, linked as
  `--platform=z80-coleco-colecorom` against `src/rt/coleco/`'s
  port-mapped VDP/controller/sound runtime library
- `6809-coco` — TRS-80 Color Computer "hello world" for `6809`, linked as
  `--platform=6809-raw` against `src/rt/coco/`'s video-RAM runtime
  library
- `z80-zxspectrum` — ZX Spectrum "hello world" for `z80`, linked as
  `--platform=z80-sna` against `src/rt/zxspectrum/`'s memory-mapped
  runtime library

`examples.mk` is shared `Makefile` plumbing (the `$(RCC)` binary path, picked
per host OS) included by each example's `Makefile`.
