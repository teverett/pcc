# rcc

`rcc` (the **r**etargetable **C** **c**ompiler) is a compiler and
linker for a small subset of C, targeting real retro hardware through
pluggable backends. Three targets can also
produce real, runnable hosted-OS executables - `i386-linux` (ELF),
`i386-windows` (PE), and `armv8a-macos` (Mach-O) - linked entirely by
rcc itself, with no external linker invoked. See
[`docs/using.md`](docs/using.md) for the `--target=<target>-<host>`
triple syntax and `--list-platforms`.

# platforms

Supported `--target` values: `pdp1`, `pdp7`, `8086`/`8088`, `6502`/`65c02`,
`68000`, `mc68328`, `pdp11`, `pdp10`, `stm8`, `m16c`/`m30626fjpfp`, `8080`, `z80`, `8008`,
`8051`/`8052`/`8031`/`8032`, `pic16`, `pic18`, `stm32`, `i386`,
`armv7a`, `armv8a`, `cortex-m3`/`cortex-m4`/`cortex-m7`,
`r10000`/`r4000`, `powerpc`, `sparc`, `vax`, `avr`, `alpha`, `saturn`,
`tms320c6713` — see
[`docs/platforms.md`](docs/platforms.md) for what each target's real
hardware is and how it's tested.

## Language

A small subset of C: `char`/`short`/`int`/`long`/`long long`, each
signed or unsigned, pointers (`T*`, with real pointee-typed arithmetic
and dereference), arrays, casts, `sizeof`, and a narrower form of
`struct` (see `docs/language.md`'s own "Structs" section). A function's
return type can also be `void` (no other use of `void` - no `void*`,
no `void` parameter - is supported), and a narrower form of `enum`
(see `docs/language.md`'s own "Enums" section). String literals
(`"like this"`) are supported - each one lowers to the address of an
anonymous, NUL-terminated `char` array holding its decoded bytes, the
same as a hand-declared `char` array would. A text-level preprocessor
handles `#include`, object-like `#define`/`#undef`, and `#ifdef`/
`#ifndef`/`#if`/`#else`/`#elif`/`#endif`, function-like and variadic
macros, stringizing, and token pasting. No unions/typedefs, no
floats.

## Documentation

- The language subset itself (types, pointers, casts, `sizeof`,
  string/char literals), its deliberate scope/limitations, bitwise
  operators, `@addr` placement, `asm { ... }`, the preprocessor, and
  rcc's own COFF object format: see
  [`docs/language.md`](docs/language.md).
- Supported platforms/backends: see
  [`docs/platforms.md`](docs/platforms.md).
- Building rcc from source: see
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
