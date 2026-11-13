# pdp7-freestanding

Same `main.c` as the other `-freestanding` examples, but compiled and
linked as a freestanding (bare-metal) binary for the `pdp7` target
instead, producing a RIM-format paper-tape image `main.rim` with no OS or
C library.

`make` runs the two-step compile then link pipeline used for freestanding
targets, both steps as `--platform=pdp7-rim` - the compile step assumes
`coff` automatically since no `--link` is given yet (see `docs/using.md`'s
own `--platform` entry). Unlike the byte-addressable `-raw` targets,
`pdp7-rim` packs real 18-bit PDP-7 words (see
`src/fileformat/Pdp7TapePacking.h`) instead of plain bytes - there's no
`pdp7-raw` platform.

Since `main.rim` has no OS to run it on, CI verifies it with a real
`open-simh` PDP-7 simulator instead: the tape is deposited at octal
10000 (the linker's own load address) via the simulator's own
`load <file> <addr>` command, run to its `hlt`, and `main()`'s return
value is read back out of the accumulator (`AC`) - see
`tests/run_tests.sh`'s own `check_pdp7()`.
