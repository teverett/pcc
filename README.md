# pcc

`pcc` is a compiler and linker for a small subset of C, targeting real
retro hardware through pluggable backends. Today it supports:

- **pdp7** — PDP-7 assembly, assembled and linked into real PDP-7
  binary formats (bootable paper tape, etc.) runnable under the
  `open-simh` PDP-7 simulator.
- **8086** — 16-bit real-mode 8086 assembly (NASM syntax), producing
  a bootable 512-byte boot-sector binary runnable directly under
  `qemu-system-i386`, or (`--format=elf`, both compile and link mode)
  a minimal flat ELF executable — same single-flat-blob model as
  every other format here (no section/symbol tables), and honestly
  incomplete the way `--format=aout` already is: a real ELF loader
  expects 32-bit protected-mode code, not 16-bit real mode, so this
  is for tooling that merely needs to recognize the file as ELF, not
  a claim that it's runnable by anything other than pcc's own
  `--format=bin` convention.
- **8088** — a deliberate, software-identical alias for `8086`, not a
  separate backend: the 8088 is the exact same instruction set as the
  8086 (its one real difference, an 8-bit external data bus instead
  of a 16-bit one, is a hardware bus-timing detail invisible to every
  instruction this compiler could ever emit), so `--target=8088`
  reuses the 8086 backend outright.
- **6502** — 6502 assembly (this project's own dialect - see
  `src/codegen/m6502/M6502Assembler.h`), producing a bare flat binary
  loaded and entered at a fixed address ($0600), with no OS or I/O
  conventions - the same "no stack, no recursion" model as every
  other target, just on a CPU with only three 8-bit registers, no
  hardware multiply/divide, and branches that can only reach ±127
  bytes (see `src/codegen/m6502/M6502Codegen.h`'s own header comment
  for how each of those gets worked around). `tests/run_tests.sh
  --target=6502` runs its own test suite against a real 6502 core via
  the `py65` package (`pip install py65`) - during development this
  was additionally cross-checked against a from-scratch pure-Python
  6502 emulator written just for that (not part of this repo).
- **65c02** — WDC 65C02 assembly, the exact same dialect/platform
  choices as `6502` above (see `src/codegen/m65c02/M65C02Assembler.h`)
  plus exactly one real CMOS-only instruction: zeroing a zero-page
  pseudo-register goes through `STZ` directly instead of the plain
  6502 backend's own "LDA #0 / STA" (a real NMOS 6502 has no "store
  zero" opcode at all) - see `src/codegen/m65c02/M65C02Codegen.h`'s
  own header comment. `tests/run_tests.sh --target=65c02` runs the
  same test suite against `py65`'s own separate CMOS core
  (`py65.devices.mpu65c02.MPU`, part of the same `py65` package as
  `6502` above) rather than its NMOS one, so the one real STZ this
  backend emits is actually checked against real 65C02 semantics.
- **68000** — 68000 assembly (this project's own dialect - see
  `src/codegen/m68k/M68kAssembler.h`), producing a bare flat binary
  the same way the 6502 backend does (loaded and entered at a fixed
  address, `$1000`, with no OS/boot ROM/vector table involved at all -
  the test harness sets the simulated CPU's PC directly). Unlike the
  6502, this target is rich enough that almost everything maps onto a
  real instruction directly: real `MULS.W`/`DIVS.W` (the *original*
  68000's only multiply/divide - 16-bit operands even though this
  backend's own `int` is 32 bits, a narrower documented limitation
  than other backends' own non-negative-only software multiply/
  divide - see `src/codegen/m68k/M68kCodegen.h`'s own header comment),
  a real variable-count shift, real signed `CMP.L`+`Bcc` (no
  sign/overflow fixup needed the way the 6502 backend's own
  comparisons do), and a real hardware stack (`A7`, real `JSR`/`RTS`)
  instead of pdp7/6502-style synthesized calls. `tests/run_tests.sh
  --target=68000` runs its own test suite against a real 68000 core
  via `qemu-system-m68k -cpu m68000` (part of the same `qemu` package
  already used for `8086`/`8088`).
- **8080** — real, standard Intel 8080 assembly and machine code (see
  `src/codegen/i8080/I8080Assembler.h`), producing a bare flat binary
  loaded and entered at a fixed address (`$0100`). Every value in
  flight lives in a handful of fixed memory cells (same idea as the
  6502's own zero-page pseudo-registers, but without that CPU's
  addressing-mode restriction, since 8080 `LHLD`/`SHLD` take any
  absolute address), with the real hardware stack (`PUSH`/`POP`,
  `CALL`/`RET`) used to save one operand across evaluating the other.
  No hardware multiply/divide and no overflow flag (`<`/`<=`/`>`/`>=`
  use the sign-bit bias trick instead of the 6502/68000's N-xor-V
  fixup — see `src/codegen/i8080/I8080Codegen.h`'s own header
  comment). `tests/run_tests.sh --target=8080` runs its own test
  suite against a real, independent Z80 core (`pip install
  z80-python`) — valid because the Z80 is object-code compatible
  with the 8080.
- **z80** — real, standard Z80 assembly and machine code (see
  `src/codegen/z80/Z80Assembler.h`), its own independent backend
  rather than a software alias of `8080` (unlike `8088`/`8086`),
  since it actually uses Z80-only instructions the 8080 doesn't have:
  real 16-bit `ADD HL,rr`/`SBC HL,rr` (so `-` and every comparison are
  one instruction instead of a byte-wise chain), direct `LD rr,(nn)`
  for BC/DE/SP (not just HL), and register-targeted `RL r`/`RR r`
  (shifting any register directly, not just the accumulator). Same
  `tests/run_tests.sh --target=z80` / `z80-python` test setup as
  `8080`, just run directly instead of as a stand-in.
- **8008** — Intel's first microprocessor, and the most primitive
  target here by a wide margin (see `src/codegen/i8008/I8008Codegen.h`'s
  own header comment): no stack pointer or PUSH/POP at all (`CALL`/
  `RET` use a real but internal, software-invisible 7-level return
  stack), no 16-bit instructions of any kind, no direct-addressing
  load/store (every memory access goes through `H`,`L`), a different
  register-code numbering than every later chip in this family, and
  rotate instructions that only ever work on the accumulator. Saving
  one operand across evaluating another therefore needs a real
  *software* stack this backend builds for itself (a fixed RAM region
  plus its own runtime stack-pointer cell), since there's no hardware
  one to use. No PyPI package exists for an 8008 core (it isn't
  object-code compatible with anything later), so
  `tests/run_tests.sh --target=8008` runs a small from-scratch 8008
  emulator written just for this, standard library only.

Every variable, temporary and literal is a single statically allocated
word: there is no stack and no recursion. The same C subset compiles
to every target — `#ifdef __PDP7__` / `#ifdef __8086__` (also defined
for `--target=8088`) / `#ifdef __6502__` (also defined for
`--target=65c02`, plus its own `#ifdef __65C02__` - see
`src/main.cpp`'s own `makeBackend()` comment) / `#ifdef __68000__` /
`#ifdef __8080__` / `#ifdef __Z80__` / `#ifdef __8008__` let source
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
make test            # runs tests/run_tests.sh against pdp7, 8086, 6502, 65c02 and 68000
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
pcc [--target=pdp7|8086|8088|6502|65c02] [--format=asm|<target-specific>] [--out=file]
    [--assembler=path] [--optimize] [--dump-ir] [--dump-asm] file.c [file2.c ...]
```

| Switch | Meaning |
| --- | --- |
| `--target=pdp7\|8086\|8088\|6502\|65c02\|68000\|8080\|z80\|8008` | Which backend to compile for. Default: `pdp7`. `8088` is a software-identical alias for `8086` (see above). |
| `--format=<fmt>` | What to produce. `asm` (the default) means "just the generated assembly text, don't assemble it". Every other value is target-specific and gets handed to that target's own assembler: `pdp7` supports `a7out`, `list`, `ptr`, `rim`, `coff`; `8086`/`8088` support `bin`, `coff`, `elf`; `6502`/`65c02`/`68000`/`8080`/`z80`/`8008` each support only `bin`. A non-`coff` format takes exactly one `.c` file (true of every format that isn't `pdp7`/`8086`/`8088` - none of the others has a `coff`). |
| `--out=file` | Where to write the result. Omit it to write to stdout; with `--format=coff` and no `--out`, output instead goes to `build/<name>.o` (one per input file). |
| `--assembler=path` | Path to the target's real external assembler, for formats that need one (not every target has an in-process assembler for every format). Defaults to each backend's own tool (`../pdp-assembler/build/bin/pdp7as` for pdp7, `nasm` for 8086/8088; `6502`/`65c02`/`68000`/`8080`/`z80`/`8008` have none - every format each supports has an in-process assembler). |
| `--optimize` | Run every registered IR optimizer (see "Optimizers" below) before codegen. Off by default. |
| `--dump-ir` | Also write each input file's intermediate representation to `build/<name>.ir`, for inspection. |
| `--dump-asm` | Also write each input file's generated assembly to `build/<name><ext>` (`.s` for pdp7, `.asm` for 8086/8088, `.s65` for 6502, `.s65c` for 65c02, `.s68` for 68000, `.s80` for 8080, `.sz80` for z80, `.s08` for 8008). |
| `-h`, `--help` | Print usage and exit. |

Real separate compilation is the only way to combine more than one
`.c` file into a final program: compile each file on its own with
`--format=coff` (one invocation, several files, each compiled
completely independently, one `build/<name>.o` per file), then
combine every `.o` with `--link` into the final program.

### Link mode

```
pcc --link [--target=pdp7|8086|8088] --format=<target-native> [--out=file] file1.o [file2.o ...]
```

| Switch | Meaning |
| --- | --- |
| `--link` | Switches from compile mode to link mode. |
| `--target=pdp7\|8086\|8088` | Which target's linker to use. Default: `pdp7`. No `6502`/`65c02`/`68000`/`8080`/`z80`/`8008` here - none of those backends supports `--link`/real separate compilation (no `--format=coff`) yet. |
| `--format=<fmt>` | Final linked output format. `pdp7` supports `a7out`, `list`, `ptr`, `rim`, `coff`, `aout`; `8086`/`8088` support `bin`, `coff`, `elf`, `aout`. |
| `--out=file` | Where to write the linked result. Omit it to write to stdout. |

Every other flag (`--assembler`, `--optimize`, `--dump-ir`,
`--dump-asm`) applies only to compile mode.

## Documentation

- `--optimize`'s IR-to-IR optimizer pipeline: see
  [`docs/optimization.md`](docs/optimization.md).
- The IR text format (`--dump-ir`'s output, and what every compile
  internally re-derives and self-checks regardless of `--dump-ir`):
  see [`docs/ir.md`](docs/ir.md).
- The shared assembly format every backend's own in-process
  assembler parses (`--format=asm`'s output, `--dump-asm`'s files):
  see [`docs/asm.md`](docs/asm.md).
