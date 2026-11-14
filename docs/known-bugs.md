# Known bugs

Defects that are understood but not yet fixed. Each entry gives the
affected code, the trigger, and the wrong behavior.

## `--target=z80-coleco` programs that multiply/divide/modulo/shift-by-variable, or deref through a pointer that spills, write through compiler-internal scratch cells placed in the same ROM-mapped cartridge image as code

Affects `src/codegen/z80/Z80Codegen.cpp`'s `declareTemp()`-based scratch
cells (`ac`/`_bc`/`cx`/`dx`/`ptr`, plus `wideScratch()`'s `a*`/`b*`/
`ret*`/`wacc` - see `genWideMul()`/`genWideDivMod()`/`genWideLoad()`/
`genWideStore()`, all of which read/write these), and
`src/linker/factory/ColecoLinkerFactory.cpp`'s/`Z80LinkerFactory.cpp`'s
shared "lay everything out in one address space starting at
`baseAddress`" model (`GenericLinker.cpp`'s `merge()`, no Harvard
code/data split wired up for this backend - contrast
`GenericLinkerConfig::dataBaseAddress`, which `pic16`/`pic18`/`avr` do
use).

Every Z80 C object - `Z80Assembler.cpp`'s `declareTemp()` emits each
scratch cell as an ordinary `lbl: db 0, 0` inside that same object's own
assembled bytes, not a separately-addressed data segment - gets linked
into the cartridge image at `$8000` and up, same as its own code. On a
real TRS-80 (`z80` target) that's fine: `$8000` is ordinary, writable
RAM there. On a real ColecoVision, `$8000`-`$FFFF` is the cartridge's
own read-only ROM - a write through one of these scratch cells (any
16-bit multiply/divide/modulo/shift-by-a-variable-amount, or a spilled
pointer dereference - see the `Z80Codegen.cpp` function references above)
is a no-op on real hardware instead of actually updating the value,
silently producing a wrong result rather than a crash. `tests/
run_coleco_rt_tests.sh`'s own z80-python-based harness can't catch this
either: its synthetic memory array has no concept of read-only regions,
so a scratch-cell write there "succeeds" even though the same write
would do nothing on real hardware.

Not yet fixed - would need either a real Harvard code(ROM)/data(RAM)
split for this backend (wiring `dataBaseAddress` through
`Z80Codegen.cpp`'s own global/temp placement the way `pic16`/`pic18`/
`avr` already do for their own Flash/RAM split) or some other way to
keep these specific scratch cells out of the cartridge image.
`src/rt/coleco/coleco.c`'s own functions
avoid every operation that would trigger this (no multiply/divide/
modulo/variable-shift, and no pointer spills past a single array
index), so they're themselves unaffected - this only bites a *caller's*
own C code once it does any of those things.

## `--target=z80` programs that write to real memory within roughly 1-2KB of `$FFFF` (while any function is still on the call stack) can silently corrupt their own return addresses/locals and hang

Affects `src/codegen/z80/Z80Codegen.cpp`'s `kStackTop` (hardcoded at
`$FF00` for every z80 platform - `--target=z80`/`z80-coleco` alike;
confirmed no `Variant`-style mechanism exists to relocate it the way
`M6502Codegen.h`'s `frameStackBase()` does for `atari2600`).

The Z80's hardware stack (`CALL`/`RET`/`PUSH`/`POP`, and every Local's/
Param's own `IX`-relative frame slot - see `Z80Codegen.h`'s own header
comment) starts at `$FF00` and grows downward from there, with no fixed
lower bound: at the moment any given instruction runs, the real
in-use stack region is `[SP, $FF00)`, and how far below `$FF00` that
reaches depends entirely on the current call depth and how many
Locals/Params are in scope - there's no compile-time constant a program
could check against. A program that writes to an address close enough
to `$FF00` (confirmed empirically: as little as ~15-250 bytes below it,
scaling with call depth) overwrites its own return address and/or
local variables instead of whatever it meant to write, with no crash or
diagnostic - just silently wrong behavior, typically an infinite loop
once the corrupted return address or loop-counter local takes effect.
`src/rt/trs80/`'s video RAM (`$3C00`-$3FFF`) and `src/rt/coleco/`'s VDP
(behind ports, not direct memory addresses) sit nowhere near this, so
neither is affected; `src/rt/cpc/`'s screen RAM genuinely does reach
`$FFFF` on real hardware, so `cpc_cls()`/`cpc_set_pixel()` deliberately
leave the last 512 bytes (`$FE00`-`$FFFF`) of that 16KB window
untouched - a comfortable, but still empirical rather than proven,
margin above the observed danger zone - see `src/rt/cpc/cpc.h`'s own
header comment.

Not yet fixed - would need `kStackTop` to become relocatable (the same
kind of `Variant` mechanism `M6502Codegen.h` already has for
`atari2600`'s own zero-page/frame-storage relocation) so a future z80
platform whose own hardware also reaches high memory could move the
stack somewhere genuinely clear of it, or some other way to reserve a
provably-safe margin instead of an empirically-chosen one.

## PDP-7 codegen can't prove a variable comparison operand is non-negative, so it still pays the full sign-dispatch cost inline

Affects `src/codegen/pdp7/Pdp7Codegen.cpp`'s `emitSignedLtCells()`/
`genCompareCells()`.

When neither operand's sign is known at compile time, `emitSignedLtCells()`
sign-dispatches on both operands via the shared `__cmp_lt` subroutine
(`pdp7RuntimeAsm()`, called via `jms`), paying that cost even for a
variable a human reader can see never actually goes negative - a `for`
loop's own induction variable (`i = 0; ...; i++`), for example.
Eliminating that dispatch would need the compiler to prove a variable's
value range at a given program point (e.g. that such a loop keeps `i`
non-negative until it exits), not just recognize a literal operand the
way the compile-time-constant case already does. That's a
value-range-analysis pass that doesn't exist anywhere in this codebase
(no `nonNeg`/`ValueRange`/range-tracking code anywhere in `src/ir` or
`src/codegen`) - a narrow, syntax-only pattern match for just the
`for(i=0;...)` shape would be easy to get subtly wrong (e.g. missing a
path that reassigns `i` inside the loop body), and a real general pass is
a substantial undertaking on its own. Still PDP-7-specific: the PDP-7 has
no condition-code/flags register, so every signed `<` needs sign-based
machinery at all, unlike i386/6502 which get a correct comparison for
free from one native compare instruction regardless of either operand's
sign.

## A packed struct can still land at a misaligned absolute address on `mips`/`r10000`

Affects `src/codegen/mips/MipsCodegen.cpp`'s frame-offset/global-placement
handling of a `@packed`-struct-typed slot (see `docs/language.md`'s own
"Structs" section for what `@packed` means).

`IRBuilder.cpp`'s `checkPackedAlignmentSafety()` only rejects a packed
layout where one member's byte offset *relative to the struct's own
start* isn't a multiple of that member's own alignment - it says nothing
about whether the struct's start itself, once placed in a stack frame or
as a global, ends up at an address whose own alignment the backend's
plain `lw`/`sw` can actually use. A packed struct's own `structAlign` is
1 (no padding), so nothing currently pads its frame offset up before
subtracting its exact byte size from `fp`: confirmed via the emitted
asm for `tests/struct_packed_basic.c`'s `struct Mixed @packed { int b;
char a; char c; }` (size 6) - its one local lands at `addiu t2,fp,-6`,
2 bytes short of a 4-byte boundary, and the `int b` member's `stind`
(a `sw`) at that address is a genuine MIPS misaligned-word trap, not
just a wrong value. The same compounds for `tests/struct_packed_array.c`:
even a base address padded to 4 bytes would still put element 1 at
`base+6`, itself 2 bytes off, since nothing pads the packed *size*
either - fixing this for real means either forcing a packed struct's
size up to its natural alignment when it's used as an array element
(which would take back some of what `@packed` is for), or giving MIPS
real unaligned-access codegen for a struct member load/store the way
`pdp7`/`pdp10`/`pdp11` already compose multi-byte access from smaller
pieces rather than trusting a single hardware instruction.

Confirmed via `tests/struct_packed_basic.c` and `tests/struct_packed_array.c`
failing on `r10000`/`mips` with a qemu monitor crash (not a wrong
value - a real trap), while every other simulator-verified backend
(`i386`, `vax`, `pdp7`, `pdp10`, `pdp11`, `armv7a`, `armv8a`/`arm64`,
`armv7a-m`/`cortex-m3`, `powerpc`, `alpha`, `saturn`, `tms320c6713`)
tolerates the same misalignment, either in hardware or via this
codegen's own byte-composed load/store. Not yet fixed; both tests are
skip-listed for `r10000` in `tests/run_tests.sh`'s `is_known_unsupported()`
rather than left as an unexplained failure.

## `tests/run_tests.sh --target=8086`/`8088` has no working test execution

Affects `tests/run_tests.sh`'s `8086|8088)` prerequisites case,
`check_8086()`, and `tests/qemu_wait.py`'s `--8086-image` handling.

`I8086LinkerFactory.cpp`'s entry stub and `I8086Codegen::generate()`'s
single-file entry sequence emit a plain flat binary loaded/entered at
`kI8086BaseAddress` (`0x1000`), with no boot-sector framing of any kind -
`--format=raw` is just `obj.textBytes`, the same convention `flat` uses
everywhere else. `check_8086()` has no way to boot that image under
`qemu-system-i386 -fda` (no `0x55 0xAA` boot signature), so
`tests/run_tests.sh --target=8086`/`8088` exits immediately with an
explanatory message instead of running (and failing) every test. Fixing
it needs a replacement way to get the flat binary running under
`qemu-system-i386` without rcc itself ever constructing a boot sector -
e.g. a tiny bootloader built and owned by the test harness (not rcc) that
chain-loads this binary via `INT 13h` and jumps to `kI8086BaseAddress`,
or switching to a different execution/verification method entirely. Not
yet fixed.

## `.github/workflows/ci.yml`'s macos job installs Python CPU simulators that no step ever runs

Affects `.github/workflows/ci.yml`.

The `macos` job's "Install Python simulator packages (py65, z80-python)"
step has a comment saying "`make test` (below) drives real, independent
CPU cores for the 6502/65c02 (py65) and 8080/z80 (z80-python) targets",
but no job invokes `make test` or `tests/run_tests.sh` anywhere in the
workflow, so that suite - and the py65/z80-python packages installed for
it - never actually run in CI. Not yet fixed.


## `pic16`/`pic18`'s `tests/pic16_pic18_extern_function_address_test.py` links with `--format=flat`, which neither target's linker factory supports

Affects `tests/pic16_pic18_extern_function_address_test.py`'s
`compileAndLink()` helper, which calls `--link --target=pic16`/`pic18`
with `--format=flat`. Both `src/linker/factory/PIC16LinkerFactory.cpp`
and `PIC18LinkerFactory.cpp` set `config.outputFormats = {"raw", "coff",
"elf", "aout"}` - `flat` isn't one of them, so every one of this test's
three cases fails immediately with `unknown --format 'flat' for --link
--target=pic16/pic18 (valid: raw coff elf aout)`, before ever reaching
the dispatch-table/extern-address behavior the test means to check.

Either the test should use one of the four formats actually supported
(most likely `raw`, matching every other bare-metal target's own
convention), or `flat` needs adding to both factories' `outputFormats` if
it's meant to be a real distinct format for these targets. Not yet
fixed.

## `--target=6809` and its CoCo runtime have no automated execution test

Affects the whole `6809` backend (`src/codegen/m6809/M6809Codegen.cpp`,
`M6809Assembler.cpp`) and `src/rt/coco/`: no verified pure-Python 6809
CPU core has been found on PyPI (unlike `z80-python`/`py65` for the
Z80/6502 families - see `tests/run_tests.sh`'s own `z80`/`6502` tool-
check arms), so `tests/run_tests.sh --target=6809` deliberately fails
loudly with a clear message instead of silently skipping or faking a
pass - see its `6809)` tool-check arm. `check_6809()` in that script is
scaffolded (same shape as `check_z80()`, reading the result out of `D`
instead of `HL` since the 6809 is big-endian) but not wired into the
dispatch `case` below it, since it can't actually run yet.

Everything else about this target has been verified as far as possible
without execution: `make build`'s own build is clean, `tests/
m6809_assembler_test.cpp` asserts exact opcode bytes against the real
MC6809 instruction encoding, `tests/m6809_link_test.py` covers object-
mode linking, and the generated assembly for the shared `tests/*.c`
battery has been read by hand and looks correct - but none of this is
the same as running real 6809 machine code. Remove this entry once a
suitable Python 6809 emulator package is found and `check_6809()` is
wired into the dispatch.

## `tests/hex_literal.c` fails on `--target=8051`/`8052`/`8031`/`8032`/`pdp1`, missing from each target's long-arithmetic skip-list

Affects `tests/run_tests.sh`'s `is_known_unsupported()`. MCS51 deliberately
has no `long`/`long long` arithmetic at all (`MCS51Codegen.cpp`'s own
`err(e.line, "'long'/'long long' arithmetic isn't supported on this
target")`), and `is_known_unsupported()` already skip-lists about twenty
other `tests/*.c` cases for `8051` on exactly that basis (`types_arrays.c`,
`test_long_arith.c`, `unsigned_literal_above_long_max.c`, etc.) - but not
`tests/hex_literal.c`, whose `unsigned long b = 0XFFUL;` hits the same
restriction. Confirmed directly: `./tests/run_tests.sh --target=8051`
fails it with `'long'/'long long' arithmetic isn't supported on this
target`, the same error every skip-listed case above gets, just without
the skip entry. Not yet fixed - needs an `8051:hex_literal.c) return 0
;;` arm alongside the existing ones (and, since the shared MCS51 backend
is identical across part numbers, the same gap presumably applies if
`8052`/`8031`/`8032` are ever added to that function's own `$TARGET`
matching instead of all falling under `8051`'s arms as they do today).

`pdp1` has the identical shape of gap for a different reason:
`Pdp1Codegen.h`'s own class comment explains it has no `long`/`long long`
support either (this one's-complement machine has no link/carry bit at
all, so a correct multi-word carry chain would need its own from-scratch
algorithm, unlike `pdp7`'s two's-complement-with-link architecture), and
`is_known_unsupported()` has no `pdp1` arm for `hex_literal.c` either.
Confirmed directly: `./tests/run_tests.sh --target=pdp1` fails
`tests/hex_literal.c` with `'long'/'long long' local variable isn't
supported on the pdp1 backend: ...`. Not yet fixed - needs a
`pdp1:hex_literal.c) return 0 ;;` arm alongside the `8051` one above.

## `make test`'s Apple II/TRS-80/ColecoVision runtime test scripts break when `$(TARGET)` is a relative path

Affects `tests/run_apple2_format_tests.sh`, `tests/run_apple2_rom_wrapper_tests.sh`,
`tests/run_trs80_rt_tests.sh`, and `tests/run_coleco_rt_tests.sh` - all
four share the same `RCC="$ROOT_DIR/build/bin/rcc"; for arg in "$@"; do
... RCC="$arg" ...; done` pattern, which takes a `path/to/rcc` argument
and stores it verbatim with no absolute-path resolution, then later does
`cd "$tmpdir"` before ever invoking `"$RCC"`. The root `Makefile`'s own
`test` target calls every one of these four with exactly `$(TARGET)` =
`build/bin/rcc` (relative, see `BIN_DIR`/`TARGET` near its top) - so once
the script's own `cd` lands it in a fresh `mktemp -d` directory, that
relative path no longer resolves to anything, and the script dies with
a plain `build/bin/rcc: No such file or directory` (bash's own
"couldn't exec" message, not one of the script's own) before running
any real check.

Confirmed directly: `./tests/run_apple2_format_tests.sh build/bin/rcc`
and `./tests/run_apple2_rom_wrapper_tests.sh build/bin/rcc`, run from the
repo root exactly the way `make test` runs them, both fail this way
(exit 127); `run_trs80_rt_tests.sh`/`run_coleco_rt_tests.sh` take the
same argument the same way and `cd` the same way, but exit earlier on
their own `z80-python not found` check in this environment, so this
wasn't directly confirmed for those two, just inferred from the
identical code shape. This is presumably why it's gone unnoticed: per
the "doesn't run `make test`/`run_tests.sh` in CI" entry above, nothing
in CI ever calls these scripts with a relative path (or at all) to
catch it. Not yet fixed - each script's `for arg in "$@"` loop needs to
resolve `$arg` to an absolute path (e.g. `RCC="$(cd "$(dirname "$arg")"
&& pwd)/$(basename "$arg")"`) before the later `cd "$tmpdir"`, the same
way `ROOT_DIR`/`SCRIPT_DIR` already resolve themselves a few lines
above.
