# Command-line switches

A build produces a single `pcc` binary (`build/bin/pcc`) that runs in
one of two modes:

- **Compile mode** (the default): turns one or more `.c` files into
  target assembly and, unless asked for raw assembly text, assembles
  that into a real binary format.
- **Link mode** (`--link`): combines separately-compiled `.o` files
  (COFF/ELF/Mach-O/a.out objects produced by `--format=coff`/`elf`/
  `macho`/`aout`) into one linked image.

See [`docs/platforms.md`](platforms.md) for the full list of targets
and what each one's own hardware/instruction set actually is.

## Compile mode

```
pcc [--target=<name>] [--format=asm|<target-specific>] [--out=file]
    [--assembler=path] [--optimize] [--dump-ir] [--dump-asm] file.c [file2.c ...]
```

| Switch | Meaning |
| --- | --- |
| `--target=<name>` | Which backend to compile for. Default: `pdp7`. One of `pdp7`, `8086`, `8088`, `6502`, `65c02`, `68000`, `mc68328`, `pdp11`, `stm8`, `m16c`, `8080`, `z80`, `8008`, `8051`, `8052`, `8031`, `8032`, `stm32`, `i386`, `pic16`, `pic18`, `r10000`, `r4000`, `cortex-m3`, `cortex-m4`, `cortex-m7`, `armv7a`, `armv8a`, `powerpc`, `sparc` — see `docs/platforms.md`. Several of these are deliberate, software-identical aliases for a sibling target rather than separate backends (`8088`→`8086`, `8052`/`8031`/`8032`→`8051`, `r4000`→`r10000`, `cortex-m4`/`cortex-m7`→`cortex-m3`) - see each one's own entry in `docs/platforms.md` and `src/main.cpp`'s own `makeBackend()` comment. |
| `--format=<fmt>` | What to produce. `asm` (the default) means "just the generated assembly text, don't assemble it". Every target supports the same uniform set of real formats, each handed to that target's own in-process assembler - no external assembler tool needed for any of them: `raw` (a complete, directly-loadable/bootable whole program - `hex` instead, for `pic16`/`pic18`'s own Intel-HEX convention), and the five real object-file formats real separate compilation uses - `coff`, `elf`, `macho`, `aout` (each a genuine relocatable object with symbols/relocations, for a future `--link` step), and `objraw` (a bare, symbol-less dump of the assembled bytes, for inspection only - it can never be `--link`ed, since it has thrown away the very `fn_main` symbol every target's linker needs). The two oldest targets keep their own historical format names instead: `pdp7` supports `a7out`, `list`, `ptr`, `rim`, `coff` (word-oriented paper-tape/listing formats, from before the format-neutral object model existed); `8086`/`8088` support `bin`, `coff`, `elf`. A non-object format (`raw`/`hex`/`bin`/`asm`, or `pdp7`'s own `a7out`/`list`/`ptr`/`rim`) takes exactly one `.c` file - only the real object formats support combining multiple independently-compiled files, via `--link` afterward. |
| `--out=file` | Where to write the result. Omit it to write to stdout; with an object format and no `--out`, output instead goes to `build/<name>.o` (one per input file). |
| `--assembler=path` | Path to the target's real *external* assembler - only meaningful for `pdp7` (defaults to `../pdp-assembler/build/bin/pdp7as`) and `8086`/`8088` (defaults to `nasm`), and only for whichever of their own formats isn't handled in-process. Every other target has an in-process assembler for every format it supports, so this flag does nothing for them. |
| `--optimize` | Run every registered IR optimizer (see `docs/optimization.md`) before codegen. Off by default. |
| `--dump-ir` | Also write each input file's intermediate representation to `build/<name>.ir`, for inspection (see `docs/ir.md`). |
| `--dump-asm` | Also write each input file's generated assembly to `build/<name><ext>`, where `<ext>` is this target's own extension - `.s` (pdp7), `.asm` (8086/8088), `.s65`/`.s65c` (6502/65c02), `.s68`/`.s328` (68000/mc68328), `.s11` (pdp11), `.s8m` (stm8), `.s16c` (m16c), `.s80`/`.sz80`/`.s08` (8080/z80/8008), `.s51` (8051/8052/8031/8032), `.pic16`/`.pic18` (pic16/pic18), `.s32` (stm32), `.s386` (i386), `.sarm` (armv7a and cortex-m3/m4/m7 - both ARM backends share this extension), `.s64` (armv8a), `.smips` (r10000/r4000), `.sppc` (powerpc), `.ssparc` (sparc). |
| `-h`, `--help` | Print usage and exit. |

Real separate compilation is the only way to combine more than one
`.c` file into a final program: compile each file on its own with
`--format=coff` (or `elf`/`macho`/`aout` - one invocation, several
files, each compiled completely independently, one `build/<name>.o`
per file), then combine every `.o` with `--link` into the final
program.

## Link mode

```
pcc --link [--target=<name>] --format=<target-native> [--out=file] file1.o [file2.o ...]
```

| Switch | Meaning |
| --- | --- |
| `--link` | Switches from compile mode to link mode. |
| `--target=<name>` | Which target's linker to use. Default: `pdp7`. Every target listed above supports `--link`; none is limited to single-file whole-program compiles. |
| `--format=<fmt>` | Final linked output format. Every target except `pdp7`/`8086`/`8088` supports the same uniform set: `flat` (just the final bytes, no container at all), `coff`, `elf`, `aout`. `pdp7` supports its own historical set instead - `a7out`, `list`, `ptr`, `rim`, `coff`, `aout`; `8086`/`8088` support `raw` (a real, directly-bootable 512-byte boot sector) in place of `flat`, plus `coff`, `elf`, `aout`. |
| `--out=file` | Where to write the linked result. Omit it to write to stdout. |

Every other flag (`--assembler`, `--optimize`, `--dump-ir`,
`--dump-asm`) applies only to compile mode.
