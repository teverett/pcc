# example3

Same `main.c` as example2, but compiled and linked as a freestanding
(bare-metal) binary for the `6502` target instead, producing a flat
`main.bin` with no OS or C library.

`make` runs the two-step compile (`--format=coff`) then link (`--format=flat`)
pipeline used for freestanding targets.
