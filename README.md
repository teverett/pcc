# pcc

`pcc` is a compiler and linker for a small subset of C, targeting real
retro hardware through pluggable backends.

## Documentation

- Supported platforms/backends: see
  [`docs/platforms.md`](docs/platforms.md).
- Building pcc from source: see
  [`docs/building.md`](docs/building.md).
- Command-line switches (compile mode and link mode): see
  [`docs/using.md`](docs/using.md).
- `--optimize`'s IR-to-IR optimizer pipeline: see
  [`docs/optimization.md`](docs/optimization.md).
- The IR text format (`--dump-ir`'s output, and what every compile
  internally re-derives and self-checks regardless of `--dump-ir`):
  see [`docs/ir.md`](docs/ir.md).
- The shared assembly format every backend's own in-process
  assembler parses (`--format=asm`'s output, `--dump-asm`'s files):
  see [`docs/asm.md`](docs/asm.md).
