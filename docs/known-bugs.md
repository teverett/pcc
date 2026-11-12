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

## `tests/run_tests.sh` never runs in CI

Affects `.github/workflows/ci.yml`.

The `macos` job's "Install Python simulator packages (py65, z80-python)"
step has a comment saying "`make test` (below) drives real, independent
CPU cores for the 6502/65c02 (py65) and 8080/z80 (z80-python) targets",
but no job invokes `make test` or `tests/run_tests.sh` anywhere in the
workflow, so that suite - and the py65/z80-python packages installed for
it - never actually run in CI.

