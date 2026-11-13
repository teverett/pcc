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
keep these specific scratch cells out of the cartridge image, neither
of which this change attempts. `src/rt/coleco/coleco.c`'s own functions
avoid every operation that would trigger this (no multiply/divide/
modulo/variable-shift, and no pointer spills past a single array
index), so they're themselves unaffected - this only bites a *caller's*
own C code once it does any of those things.

## PDP-7 codegen can't prove a variable comparison operand is non-negative, so it still pays the full sign-dispatch cost inline

Affects `src/codegen/pdp7/Pdp7Codegen.cpp`'s `emitSignedLtCells()`/
`genCompareCells()`.

Originally filed as a wider "lowers small-constant comparisons
generically, compounded by zero cross-call-site code sharing" entry
(`i < 10` in a `for` loop costing 15 words against a hand-written
original's much smaller equivalent - see UnixV0-C's own `awake()`
comparison). Two of that entry's three contributing costs have since been
fixed in full:

1. **The same-sign subtract step re-negating a *compile-time constant*
   operand at runtime** (`lac B`/`cma`/`tad __one`/`dac`, 4 words) instead
   of just adding its already-known negation (`tad <literal>`, 1 word).
   `genCompareCells()` now precomputes that negated literal (the same
   "add a negative literal directly" idiom `narrowAC()`/etc. already use
   elsewhere in this file) whenever the comparison's `B` side is an
   `ir::ExprKind::IntLit`, and `emitSignedLtCells()` uses it instead - see
   `tests/run_pdp7_compare_constant_tests.sh`, which checks the runtime
   negation (a `cma`) is gone for that case and still present for a plain
   variable-vs-variable comparison. Cuts `i < 10`'s own cost from 15
   words to 11.
2. **Zero cross-call-site code sharing**: when *neither* operand's sign
   is known at compile time (the most expensive shape - both of
   `emitSignedLtCells`'s dispatch arms, each needing their own sign test
   on `B` too, ~21 words), the full sequence used to be inlined again at
   every such comparison, program-wide, with nothing shared between
   them. It's now factored into one `__cmp_lt` subroutine
   (`pdp7RuntimeAsm()`, same conditionally-emitted-trailer convention as
   `__push`/`__pop`/`__divmod`), called via `jms` from every such site
   instead - ~8 words per call site against a one-time ~25-word shared
   body, net smaller as soon as it's used twice anywhere in the program.
   See `tests/run_pdp7_shared_compare_subroutine_tests.sh` (two different
   functions both `jms __cmp_lt`, and `__cmp_lt:` itself appears exactly
   once).

What's still open, and why it's a different, harder problem: `i < 10`'s
own remaining 11 words are mostly the "is the variable operand itself
negative" dispatch (`emitSignedLtCells`'s `signA == -1` branch) -
necessary in general, since a plain `int` can be negative, but wasted
work for a loop induction variable like `i` above, which a human reader
can see never actually goes negative here. Eliminating that dispatch -
the "known-safe constant" half of the original entry's title - would need
the compiler to prove a variable's value range at a given program point
(e.g., tracking that a `for` loop's own `i = 0; ...; i++` keeps `i`
non-negative until the loop exits), not just recognize a literal operand
the way fix 1 above does. That's a value-range-analysis pass that doesn't
exist anywhere in this codebase (confirmed by inspection: no `nonNeg`/
`ValueRange`/range-tracking code anywhere in `src/ir` or `src/codegen`) -
deliberately not attempted here, since a narrow, syntax-only pattern
match for just the `for(i=0;...)` shape would be easy to get subtly wrong
(e.g. missing a path that reassigns `i` inside the loop body) in a way
nothing in this codebase would catch, and a real general pass is a
substantial undertaking on its own, not a small addition alongside the
other two fixes. Still PDP-7-specific either way: the PDP-7 has no
condition-code/flags register, so every signed `<` needs sign-based
machinery at all, unlike i386/6502 which get a correct comparison for
free from one native compare instruction regardless of either operand's
sign.

(A third pattern originally filed alongside this one - every array
reference re-deriving its own base address from scratch at each use - has
also been fixed in full, by the `AddressHoisting` optimizer pass
(`src/optimizers/address_hoisting/`, see
`tests/run_pdp7_address_hoisting_tests.sh`), which now computes a
repeatedly-accessed array's base address once per function instead of at
every access, on every backend, not just pdp7. A fourth, unrelated
pattern - uninitialized scalar locals being zeroed twice - has also been
fixed, by deleting `Pdp7Codegen.cpp`'s own defensive zero-store, which was
PDP-7-codegen-specific, not an IR/optimizer gap.)

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

## Most backends mis-scaled an array index for an element whose width isn't 1/2/4/8 bytes

Affected `src/codegen/{mips,powerpc,alpha,tms320c6713,x86_8086,avr,mcs51,
i8008,m6502,m65c02,i8080,z80}/*Codegen.{h,cpp}` - every backend with this
`elemShift()`/doubling-count pattern other than `i386`, `vax`, `pdp7`,
`pdp10`, `pdp11`, `armv7a`/`armv8a`/`arm64`, and `armv7a-m`/`cortex-m`,
which this subset's earlier work already covered.

Each affected backend's array-index address computation (`genIndexAddr()`
or equivalent) scaled the index by the element's byte width via a
shift/doubling count derived from `widthOf()`, with a fallback for any
width other than 1/2/4/8 bytes that was simply wrong: it treated the
width as if it were 8 regardless of what it actually was (one extra
shift/doubling step past the `4` case), silently computing the wrong
element address - confirmed via a direct pointer-arithmetic check
(`&arr[1] - &arr[0]` came back 8 regardless of the element's real
width), not just a read-back-the-value check: the two are not the same
test. A read-back-the-value check can pass even though the address is
wrong, because the same wrong address is used consistently for both the
write and the read-back within one compiled program, and the extra
bytes it strays into are usually still this same local array's own
otherwise-unused frame slop or another dead stack slot, not something
that gets overwritten before the mismatched read-back happens; this is
exactly why `tests/struct_mixed_width_fields.c` and
`tests/struct_packed_array.c` (both read-back-the-value checks) falsely
appeared to pass on `m6502`/`m65c02`/`i8008` before the pointer check
below was written - their 2-byte `int` happened to make those two
tests' particular struct size a coincidental power of two (4), even
though the bug was just as present there.
`MCS51Codegen::genIndexAddr()` was worse still - it doubled the index
exactly once for *any* width other than 1, which already mis-scaled a
plain `long`/`long long` array (width 4/8) even without a struct
anywhere involved.

Before `@packed`/struct-array support, every element type's width was
always exactly 1/2/4/8 (`char`/`short`/`int`/`long`/`long long`), so this
never fired. A struct array can: a packed struct's size is whatever its
members add up to, and even an *unpacked* struct's size only happens to
be a power of two by coincidence (e.g. three `int` members packs to 12
bytes unpacked too); a struct of 1/2/3 `char` members is a reliable
repro on every target regardless of its own `int` width, which is why
`tests/struct_nonpow2_array_stride.c` (new, a 3-`char` struct) uses that
shape and checks the real pointer stride directly, not just a value
read back.

Fixed, with a real multiply in place of the shift - `mullw`/`mulq`/`mul`/
`imul`/the shared software multiply-by-register routine each backend
already had for a scalar `*`, as appropriate per target, except
`i8080`/`z80`: there, the second multiplicand (the element width) is
always a compile-time constant, so `I8080Codegen.cpp`'s and
`Z80Codegen.cpp`'s own `genIndexAddr()` instead emit a fully unrolled
compile-time shift-and-add (double a scratch pseudo-register, add it in
for each set bit of the width) rather than reuse the runtime
bit-serial `genMul()` built for two operands that are both unknown
until runtime. Verified via each backend's own qemu/`open-simh`/
`z80-python` simulator against `tests/struct_nonpow2_array_stride.c` and
the rest of `tests/struct_*.c`, including `i8080`/`z80` (the
`z80-python` package wasn't installed when this was first written up,
leaving those two unverified; it now is). `sparc`, `m16c`, `pic16`,
`pic18` still have no working simulator wired into `tests/run_tests.sh`
at all to verify a fix against, and remain unverified (not necessarily
unfixed - their `elemShift()` has the same shape as the others, just
never confirmed against real execution).

## `tests/run_tests.sh --target=8086`/`8088` has no working test execution

Affects `tests/run_tests.sh`'s `8086|8088)` prerequisites case,
`check_8086()`, and `tests/qemu_wait.py`'s `--8086-image` handling.

`I8086LinkerFactory.cpp`'s entry stub and `I8086Codegen::generate()`'s
single-file entry sequence used to assemble a BIOS-bootable 512-byte boot
sector (`ORG 0x7C00`, padded/signed with `0x55 0xAA` by the old
`RawFormat.cpp`), which `check_8086()` ran by handing the whole image to
`qemu-system-i386 -fda`, with `qemu_wait.py`'s `i8086_end_pattern()`
byte-matching that exact entry sequence to detect completion.

Producing a boot-sector image is out of scope for this project (same
reasoning as `i386`'s own hosted-vs-bare-metal split), so both entry
sequences now emit a plain flat binary loaded/entered at
`kI8086BaseAddress` (`0x1000`) with no boot-sector framing at all -
`--format=raw` is just `obj.textBytes`, the same convention `flat` uses
everywhere else. `-fda` can no longer boot that image (no `0x55 0xAA`
signature), so `tests/run_tests.sh --target=8086`/`8088` now exits
immediately with an explanatory message instead of running (and failing)
every test - testing this target is deliberately disabled for now rather
than left silently broken. Fixing it needs a replacement way to get the
flat binary running under `qemu-system-i386` without rcc itself ever
constructing a boot sector - e.g. a tiny bootloader built and owned by
the test harness (not rcc) that chain-loads this binary via `INT 13h` and
jumps to `kI8086BaseAddress`, or switching to a different execution/
verification method entirely. Not yet fixed.


Affects `.github/workflows/ci.yml`.

The `macos` job's "Install Python simulator packages (py65, z80-python)"
step has a comment saying "`make test` (below) drives real, independent
CPU cores for the 6502/65c02 (py65) and 8080/z80 (z80-python) targets",
but no job invokes `make test` or `tests/run_tests.sh` anywhere in the
workflow, so that suite - and the py65/z80-python packages installed for
it - never actually run in CI.


## `pic16`/`pic18`'s `tests/pic16_pic18_extern_function_address_test.py` links with `--format=flat`, which neither target's linker factory supports

Affects `tests/pic16_pic18_extern_function_address_test.py`'s
`compileAndLink()` helper, which calls `--link --target=pic16`/`pic18`
with `--format=flat`. Both `src/linker/factory/PIC16LinkerFactory.cpp`
and `PIC18LinkerFactory.cpp` set `config.outputFormats = {"raw", "coff",
"elf", "aout"}` - `flat` isn't one of them, so every one of this test's
three cases fails immediately with `unknown --format 'flat' for --link
--target=pic16/pic18 (valid: raw coff elf aout)`, before ever reaching
the dispatch-table/extern-address behavior the test means to check.

Pre-existing; unrelated to any 6809/CoCo work. Either the test should use
one of the four formats actually supported (most likely `raw`, matching
every other bare-metal target's own convention), or `flat` needs adding
to both factories' `outputFormats` if it's meant to be a real distinct
format for these targets. Not yet fixed.

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
