# m30626fjpfp-freestanding

Compiles and links `main.c` as a freestanding (bare-metal) binary for the
`m30626fjpfp` target, producing a flat `main.bin` with no OS or C library.

`make` runs the two-step compile (`--format=coff`) then link (`--format=raw`)
pipeline used for freestanding targets.
