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
make                # uses Homebrew's antlr4-cpp-runtime
make test            # runs tests/run_tests.sh against both targets
```

The generated C++ lexer/parser code (`src/generated/`, from
`grammar/*.g4`) is checked into git - `make` never invokes the real
ANTLR tool itself, so building pcc needs only the ANTLR4 C++
*runtime* (headers/lib), not Java or the ANTLR jar. See the
Makefile's own header comment for how to point it at a non-Homebrew
runtime install instead.

After editing a `grammar/*.g4` file, regenerate `src/generated/` by
hand with `scripts/generate-grammars.sh` (or `make generate`, which
just runs that script), review the diff, and commit both together.

## Command-line switches

### Compile mode

```
pcc [--target=pdp7|8086] [--format=asm|<target-specific>] [--out=file]
    [--assembler=path] [--optimize] [--dump-ir] [--dump-asm] file.c [file2.c ...]
```

| Switch | Meaning |
| --- | --- |
| `--target=pdp7\|8086` | Which backend to compile for. Default: `pdp7`. |
| `--format=<fmt>` | What to produce. `asm` (the default) means "just the generated assembly text, don't assemble it". Every other value is target-specific and gets handed to that target's own assembler: `pdp7` supports `a7out`, `list`, `ptr`, `rim`, `coff`; `8086` supports `bin`, `coff`, `elf`. A non-`coff` format takes exactly one `.c` file. |
| `--out=file` | Where to write the result. Omit it to write to stdout; with `--format=coff` and no `--out`, output instead goes to `build/<name>.o` (one per input file). |
| `--assembler=path` | Path to the target's real external assembler, for formats that need one (not every target has an in-process assembler for every format). Defaults to each backend's own tool (`../pdp-assembler/build/bin/pdp7as` for pdp7, `nasm` for 8086). |
| `--optimize` | Run every registered IR optimizer (see "Optimizers" below) before codegen. Off by default. |
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
| `--format=<fmt>` | Final linked output format. `pdp7` supports `a7out`, `list`, `ptr`, `rim`, `coff`, `aout`; `8086` supports `bin`, `coff`, `elf`, `aout`. |
| `--out=file` | Where to write the linked result. Omit it to write to stdout. |

Every other flag (`--assembler`, `--optimize`, `--dump-ir`,
`--dump-asm`) applies only to compile mode.

## Optimizers

`--optimize` runs a pipeline of target-independent IR-to-IR passes
(`src/optimizers/`, each implementing the `Optimizer` interface in
`src/optimizers/Optimizer.h`) between `IRBuilder` and whichever
backend is selected — since every backend-specific decision
(instruction selection, register/stack strategy) happens strictly
after the IR exists, an optimizer written once benefits both targets.

Today there's one:

- **`DeadCodeElimination`** (`src/optimizers/DeadCodeElimination.cpp`)
  — removes statements that can never run (anything after a `return`
  in the same block, bare `;` statements) and local variables that
  are declared but never read, written, or had their address taken,
  dropping their storage entirely unless their own initializer has a
  side effect (a call), in which case it's kept as a bare expression
  statement instead.

Not yet attempted, because they'd need a real control-flow graph
and/or constant folding first: dead-*store* elimination, constant-
condition `if`/`while` removal, and CSE.

### IR grammar

The IR's own printed text (what `--dump-ir` writes, and what every
compile internally re-derives regardless of `--dump-ir` — see
`src/ir/IRPrinter.h`) is a real, independently parseable language,
described by `grammar/PccIRLexer.g4` + `grammar/PccIRParser.g4` (generated
via ANTLR, same as the C frontend and the `PccAsm` grammar every
backend's in-process assembler shares). `src/ir/IRValidator.h` parses
every program's own printed IR against that generated parser on
*every* compile — optimized or not — as a standing self-check that
`IRPrinter.cpp` and the grammar describing it haven't drifted apart;
a mismatch throws as an internal pcc bug, never a user source error.
