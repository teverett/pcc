# examples

Small, buildable programs demonstrating `rcc` usage, each in its own
directory with a `Makefile` and a `README.md` of its own:

- `hosted-hello` — hosted executable for the current desktop OS (macOS/Linux/Windows)
- `m30626fjpfp-freestanding` — freestanding binary for the `m30626fjpfp` target
- `6502-freestanding` — freestanding binary for the `6502` target
- `68000-freestanding` — freestanding binary for the `68000` target
- `6502-dos33` — Apple ][ "hello world" for `6502`, linked as
  `--platform=6502-prodos` against `src/rt/apple2/`'s ROM-call runtime
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

`examples.mk` is shared `Makefile` plumbing (the `$(RCC)` binary path, picked
per host OS) included by each example's `Makefile`.
