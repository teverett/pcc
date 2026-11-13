# Known bugs

Defects that are understood but not yet fixed. Each entry gives the
affected code, the trigger, and the wrong behavior.

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
i8008,m6502,m65c02}/*Codegen.{h,cpp}` - every backend with this
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

Fixed, with a real multiply (`mullw`/`mulq`/`mul`/`imul`/the shared
software multiply-by-register routine each of these backends already
had for a scalar `*`, as appropriate per target) in place of the shift,
on every backend listed above - each verified via its own qemu or
`open-simh` simulator against `tests/struct_nonpow2_array_stride.c` and
the rest of `tests/struct_*.c`. `sparc`, `m16c`, `pic16`, `pic18` still
have no working simulator wired into `tests/run_tests.sh` at all to
verify a fix against, and `8080`/`z80` need the `z80-python` package
(not installed in this environment) to run theirs; these four remain
unverified (not necessarily unfixed - their `elemShift()` has the same
shape as the others, just never confirmed against real execution).

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

