# pcc

`pcc` is a compiler and linker for a small subset of C, targeting real
retro hardware through pluggable backends. Today it supports two
targets:

- **pdp7** — PDP-7 assembly, assembled and linked into real PDP-7
  binary formats (bootable paper tape, etc.) runnable under the
  `open-simh` PDP-7 simulator.
- **8086** — 16-bit real-mode 8086 assembly (NASM syntax), producing a
  bootable 512-byte boot-sector binary runnable directly under
  `qemu-system-i386`.

Every variable, temporary and literal is a single statically allocated
word: there is no stack and no recursion. The same C subset compiles
to either target — `#ifdef __PDP7__` / `#ifdef __8086__` let source
code branch on which one it's being built for.

A build produces a single `pcc` binary (`build/bin/pcc`) that runs in
one of two modes:

- **Compile mode** (the default): turns one or more `.c` files into
  target assembly and, unless asked for raw assembly text, assembles
  that into a real binary format.
- **Link mode** (`--link`): combines separately-compiled `.o` files
  (COFF objects produced by `--format=coff`) into one linked image.

## Building

```
make                # uses Homebrew's antlr/antlr4-cpp-runtime
make test            # runs tests/run_tests.sh against both targets
```

See the Makefile's own header comment for how to point it at a
vendored (non-Homebrew) ANTLR tool + C++ runtime instead.

## Command-line switches

### Compile mode

```
pcc [--target=pdp7|8086] [--format=asm|<target-specific>] [--out=file]
    [--assembler=path] [--dump-ir] [--dump-asm] file.c [file2.c ...]
```

| Switch | Meaning |
| --- | --- |
| `--target=pdp7\|8086` | Which backend to compile for. Default: `pdp7`. |
| `--format=<fmt>` | What to produce. `asm` (the default) means "just the generated assembly text, don't assemble it". Every other value is target-specific and gets handed to that target's own assembler: `pdp7` supports `a7out`, `list`, `ptr`, `rim`, `coff`; `8086` supports `bin`, `coff`. A non-`coff` format takes exactly one `.c` file. |
| `--out=file` | Where to write the result. Omit it to write to stdout; with `--format=coff` and no `--out`, output instead goes to `build/<name>.o` (one per input file). |
| `--assembler=path` | Path to the target's real external assembler, for formats that need one (not every target has an in-process assembler for every format). Defaults to each backend's own tool (`../pdp-assembler/build/bin/pdp7as` for pdp7, `nasm` for 8086). |
| `--dump-ir` | Also write each input file's intermediate representation to `build/<name>.ir`, for inspection. |
| `--dump-asm` | Also write each input file's generated assembly to `build/<name><ext>` (`.s` for pdp7, `.asm` for 8086). |
| `-h`, `--help` | Print usage and exit. |

Real separate compilation is the only way to combine more than one
`.c` file into a final program: compile each file on its own with
`--format=coff` (one invocation, several files, each compiled
completely independently, one `build/<name>.o` per file), then
combine every `.o` with `--link` into the final program.

### Link mode

```
pcc --link [--target=pdp7|8086] --format=<target-native> [--out=file] file1.o [file2.o ...]
```

| Switch | Meaning |
| --- | --- |
| `--link` | Switches from compile mode to link mode. |
| `--target=pdp7\|8086` | Which target's linker to use. Default: `pdp7`. |
| `--format=<fmt>` | Final linked output format. `pdp7` supports `a7out`, `list`, `ptr`, `rim`, `coff`, `aout`; `8086` supports `bin`, `coff`, `aout`. |
| `--out=file` | Where to write the linked result. Omit it to write to stdout. |

Every other flag (`--assembler`, `--dump-ir`, `--dump-asm`) applies
only to compile mode.
