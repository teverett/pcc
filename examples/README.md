# examples

Small, buildable programs demonstrating `rcc` usage, each in its own
directory with a `Makefile` and a `README.md` of its own:

- `hosted-hello` — hosted executable for the current desktop OS (macOS/Linux/Windows)
- `m30626fjpfp-freestanding` — freestanding binary for the `m30626fjpfp` target
- `6502-freestanding` — freestanding binary for the `6502` target
- `6502-dos33` — Apple ][ "hello world" for `6502`, linked as `--format=prodos`
  against `src/rt/apple2/`'s ROM-call runtime library
- `8086-msdos` — MS-DOS "hello world" for `8086-dos`, linked as `--format=raw`
  (a directly-runnable `.com`) against `src/rt/msdos/`'s `INT 21h`
  runtime library

`examples.mk` is shared `Makefile` plumbing (the `$(RCC)` binary path, picked
per host OS) included by each example's `Makefile`.
