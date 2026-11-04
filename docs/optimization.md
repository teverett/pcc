# Optimizers

`--optimize` runs a pipeline of target-independent IR-to-IR passes
(`src/optimizers/`, each implementing the `Optimizer` interface in
`src/optimizers/Optimizer.h`) between `IRBuilder` and whichever
backend is selected — since every backend-specific decision
(instruction selection, register/stack strategy) happens strictly
after the IR exists, an optimizer written once benefits every target
(see `docs/platforms.md` for the full list).

Today there are three, run in this order (`src/main.cpp`):

- **`ConstantFolding`** (`src/optimizers/ConstantFolding.cpp`) —
  bottom-up folds any unary or binary expression whose operand(s) are
  already `IntLit`s into a single `IntLit` (e.g. `((2 + 3) * 4)` →
  `20`). Leaves an op unfolded when evaluating it on the host would
  require undefined behavior the real target's own instruction
  handles fine instead — division/modulo by a constant zero, or a
  shift by an out-of-range amount. Runs first because it often turns
  a once-variable initializer or condition into a literal that
  `DeadCodeElimination` can then reason about.

- **`AlgebraicSimplification`**
  (`src/optimizers/AlgebraicSimplification.cpp`) — strength reduction:
  rewrites a binary expression with exactly one `IntLit` operand into
  a cheaper equivalent that holds for every possible value of the
  other operand — `x * 2`/`2 * x` → `x << 1`, `x * -1` → `-x`, `x / 1`
  → `x`, `x / -1` → `-x`, `x + 0`/`0 + x` → `x`, `x - 0` → `x`,
  `x << 0`/`x >> 0` → `x`, `x | 0`/`0 | x` → `x`, `x ^ 0`/`0 ^ x` → `x`.
  This is strictly narrower than `ConstantFolding`: it never touches a
  both-literal expression (that's already folded by the time this
  pass runs) and never drops the non-literal operand's own
  subexpression, so a side effect inside it (a call) still runs
  exactly once either way. Deliberately *not* rewritten, because it
  would have to drop that operand entirely to reach the answer, which
  is only safe if it has no side effect: `x * 0`, `x % 1`, `x & 0`.
  Also not rewritten: `x / <power of two>` / `x % <power of two>` as a
  shift/mask — truncating (round-toward-zero) division isn't
  bit-identical to an arithmetic right-shift for a negative `x`, and
  this pass has no way to know `x`'s sign at compile time in general.
  Helps every target, but especially one like pdp7 with no hardware
  multiply/divide shortcut for small constants.

- **`DeadCodeElimination`** (`src/optimizers/DeadCodeElimination.cpp`)
  — removes statements that can never run (anything after a `return`
  in the same block, bare `;` statements) and local variables that
  are declared but never read, written, or had their address taken,
  dropping their storage entirely unless their own initializer has a
  side effect (a call), in which case it's kept as a bare expression
  statement instead.

Not yet attempted, because they'd need a real control-flow graph
first: dead-*store* elimination, constant-condition `if`/`while`
removal, and CSE.

## IR grammar

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
