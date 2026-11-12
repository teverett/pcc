# example3

Same `main.c` as example2, but compiled and linked as a freestanding
(bare-metal) binary for the `6502` target instead, producing a flat
`main.bin` with no OS or C library.

`make` runs the two-step compile (`--format=coff`) then link (`--format=flat`)
pipeline used for freestanding targets.

Since `main.bin` has no OS to run it on, CI verifies it with py65's 6502 MPU
core instead: the binary is loaded at `0x0600` (the `--format=flat` linker's
load address) and stepped until it reaches the linker's own `halt: jmp halt`
loop (detected as the PC repeating), at which point `main()`'s return value
is read back from zero-page addresses 2/3 (little-endian, sign-extended).
