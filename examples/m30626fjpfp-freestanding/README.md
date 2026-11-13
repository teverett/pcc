# m30626fjpfp-freestanding

Compiles and links `main.c` as a freestanding (bare-metal) binary for the
`m30626fjpfp` target, producing a flat `main.bin` with no OS or C library.

`make` runs the two-step compile then link pipeline used for freestanding
targets, both steps as `--platform=m30626fjpfp-raw` - the compile step
assumes `coff` automatically since no `--link` is given yet (see
`docs/using.md`'s own `--platform` entry).
