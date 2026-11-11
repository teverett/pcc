# examples

Small, buildable programs demonstrating `rcc` usage, each in its own
directory with a `Makefile` and a `README.md` of its own:

- `example1` — hosted executable for the current desktop OS (macOS/Linux/Windows)
- `example2` — freestanding binary for the `m30626fjpfp` target
- `example3` — freestanding binary for the `6502` target

`examples.mk` is shared `Makefile` plumbing (the `$(RCC)` binary path, picked
per host OS) included by each example's `Makefile`.
