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

## Most backends mis-scale an array index for an element whose width isn't 1/2/4/8 bytes

Affects `src/codegen/{mips,powerpc,sparc,alpha,stm8,m16c,tms320c6713,
x86_8086,pic16,pic18,stm32,avr,mcs51,i8080,z80,i8008,m6502,m65c02}/*Codegen.{h,cpp}`
(every backend with this `elemShift()`/doubling-count pattern other than
`i386`, `vax`, `pdp7`, `pdp10`, `pdp11`, `armv7a`/`armv8a`/`arm64`, and
`armv7a-m`/`cortex-m`, which are fixed - see below).

Each affected backend's array-index address computation (`genIndexAddr()`
or equivalent) scales the index by the element's byte width via a
shift/doubling count derived from `widthOf()`, with a fallback for any
width other than 1/2/4/8 bytes that's simply wrong: it treats the width
as if it were 8 regardless of what it actually is (one extra
shift/doubling step past the `4` case), silently computing the wrong
element address - confirmed via a direct pointer-arithmetic check
(`&arr[1] - &arr[0]` comes back 8 regardless of the element's real
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
though the bug is just as present there.
`MCS51Codegen::genIndexAddr()` is worse still - it doubles the index
exactly once for *any* width other than 1, which already mis-scales a
plain `long`/`long long` array (width 4/8) even without a struct
anywhere involved.

Before `@packed`/struct-array support, every element type's width was
always exactly 1/2/4/8 (`char`/`short`/`int`/`long`/`long long`), so this
never fired. A struct array now can: a packed struct's size is whatever
its members add up to, and even an *unpacked* struct's size only
happens to be a power of two by coincidence (e.g. three `int` members
packs to 12 bytes unpacked too); a struct of 1/2/3 `char` members is a
reliable repro on every target regardless of its own `int` width, which
is why `tests/struct_nonpow2_array_stride.c` (new, a 3-`char` struct)
uses that shape and checks the real pointer stride directly, not just a
value read back.

Confirmed and fixed, with a real multiply (or - on `pdp11` only, which
has no native multiply wired into this codegen's own index-scaling path
- the registers it already uses for a scalar `*`) in place of the shift,
each verified via that target's own qemu or `open-simh` simulator
against `tests/struct_nonpow2_array_stride.c` and the rest of
`tests/struct_*.c`, on: `i386`, `vax`, `pdp7`, `pdp10`, `pdp11`,
`armv7a`, `armv8a`/`arm64`, `armv7a-m`/`cortex-m3`. `powerpc`, `alpha`,
`saturn`, `tms320c6713`, `r10000`/`mips`, `8086`/`8088`, and `8051` were
confirmed still affected (same qemu/`open-simh` simulators, same test)
but not yet fixed - each needs its own per-ISA scaling fix verified the
same way, which is a substantial separate undertaking given how many
backends are involved; `sparc`, `m16c`, `pic16`, `pic18` have no working
simulator wired into `tests/run_tests.sh` at all, and `8080`/`z80` need
the `z80-python` package (not installed in this environment) to run
theirs.

## `tests/run_tests.sh` never runs in CI

Affects `.github/workflows/ci.yml`.

The `macos` job's "Install Python simulator packages (py65, z80-python)"
step has a comment saying "`make test` (below) drives real, independent
CPU cores for the 6502/65c02 (py65) and 8080/z80 (z80-python) targets",
but no job invokes `make test` or `tests/run_tests.sh` anywhere in the
workflow, so that suite - and the py65/z80-python packages installed for
it - never actually run in CI.

