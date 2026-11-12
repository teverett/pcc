# Known bugs

Defects that are understood but not yet fixed. Each entry gives the
affected code, the trigger, and the wrong behavior.

## PDP-7 codegen still dispatches on a variable operand's sign at runtime even when it's provably non-negative

Affects `src/codegen/pdp7/Pdp7Codegen.cpp`'s `emitSignedLtCells()`/
`genCompareCells()`.

Originally filed as a wider "lowers small-constant comparisons
generically" entry (`i < 10` in a `for` loop costing 15 words against a
hand-written original's much smaller equivalent - see UnixV0-C's own
`awake()` comparison). Since then, the part of that cost which was purely
mechanical - the same-sign subtract step re-negating a *compile-time
constant* operand at runtime (`lac B`/`cma`/`tad __one`/`dac`, 4 words)
instead of just adding its already-known negation (`tad <literal>`, 1
word) - has been fixed: `genCompareCells()` now precomputes that negated
literal (the same "add a negative literal directly" idiom
`narrowAC()`/etc. already use elsewhere in this file) and
`emitSignedLtCells()` uses it instead, whenever the comparison's `B` side
is an `ir::ExprKind::IntLit` (see `tests/run_pdp7_compare_constant_tests.sh`,
which checks the runtime negation - a `cma` - is gone for that case and
still present for a plain variable-vs-variable comparison). This cuts
`i < 10`'s own cost from 15 words to 11.

What's left, and why it's a different, harder problem: the remaining 11
words are mostly the "is the variable operand itself negative" dispatch
(`emitSignedLtCells`'s `signA == -1` branch) - necessary in general, since
a plain `int` can be negative, but wasted work for a loop induction
variable like `i` above, which a human reader can see never actually goes
negative here. Eliminating that dispatch would need the compiler to prove
a variable's value range at a given program point (e.g., tracking that a
`for` loop's own `i = 0; ...; i++` keeps `i` non-negative until the loop
exits) - a value-range-analysis pass that doesn't exist anywhere in this
codebase (confirmed by inspection: no `nonNeg`/`ValueRange`/range-tracking
code anywhere in `src/ir` or `src/codegen`), not a small addition. Still
PDP-7-specific either way: the PDP-7 has no condition-code/flags register,
so every signed `<` needs this sign-based machinery at all, unlike i386/
6502 which get a correct comparison for free from one native compare
instruction regardless of either operand's sign.

(Two other patterns originally filed alongside this one - every array
reference re-deriving its own base address from scratch at each use, and
uninitialized scalar locals being zeroed twice - have been fixed in full:
the first by the `AddressHoisting` optimizer pass
(`src/optimizers/address_hoisting/`, see `tests/run_pdp7_address_hoisting_tests.sh`),
which now computes a repeatedly-accessed array's base address once per
function instead of at every access, on every backend, not just pdp7; the
second by deleting `Pdp7Codegen.cpp`'s own defensive zero-store, which was
PDP-7-codegen-specific, not an IR/optimizer gap.)

## `tests/run_tests.sh` never runs in CI

Affects `.github/workflows/ci.yml`.

The `macos` job's "Install Python simulator packages (py65, z80-python)"
step has a comment saying "`make test` (below) drives real, independent
CPU cores for the 6502/65c02 (py65) and 8080/z80 (z80-python) targets",
but no job invokes `make test` or `tests/run_tests.sh` anywhere in the
workflow, so that suite - and the py65/z80-python packages installed for
it - never actually run in CI.

