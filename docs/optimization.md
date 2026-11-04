# Optimizers

`--optimize` runs a pipeline of target-independent IR-to-IR passes
(`src/optimizers/`, each implementing the `Optimizer` interface in
`src/optimizers/Optimizer.h`) between `IRBuilder` and whichever
backend is selected — since every backend-specific decision
(instruction selection, register/stack strategy) happens strictly
after the IR exists, an optimizer written once benefits every target
(see `docs/platforms.md` for the full list).

Today there are five, run in this order (`src/main.cpp`):

- **`ConstantFolding`** (`src/optimizers/constant_folding/ConstantFolding.cpp`) —
  bottom-up folds any unary or binary expression whose operand(s) are
  already `IntLit`s into a single `IntLit` (e.g. `((2 + 3) * 4)` →
  `20`). Leaves an op unfolded when evaluating it on the host would
  require undefined behavior the real target's own instruction
  handles fine instead — division/modulo by a constant zero, or a
  shift by an out-of-range amount. Runs first because it often turns
  a once-variable initializer or condition into a literal that
  `DeadCodeElimination` can then reason about.

- **`AlgebraicSimplification`**
  (`src/optimizers/algebraic_simplification/AlgebraicSimplification.cpp`) — strength reduction:
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

- **`LocalCSE`** (`src/optimizers/local_cse/LocalCSE.cpp`) — simple local
  common-subexpression elimination / redundant-load elimination:
  purely syntactic (not flow-sensitive) matching of identical
  `Binary`/`Index` subexpressions within one `Block`'s own statement
  list (an `If`/`While`'s own `cond` counts as part of its enclosing
  block; its `then`/`else`/loop body gets its own separate, unrelated
  scope). On a repeated, side-effect-free occurrence, the *first* one
  is wrapped in place with an assignment into a fresh temporary (still
  evaluating at exactly its original program point - nothing is ever
  moved), and every later occurrence becomes a plain read of it
  instead of recomputing it. An occurrence stops being eligible to
  match again the moment something could have changed its value: a
  direct write to a `Slot` it depends on drops only that occurrence,
  while a `Call`/`CallPtr`/`Intrinsic`/`DerefAssign` conservatively
  drops every occurrence recorded so far in the scope. Runs after
  `AlgebraicSimplification` so it sees each subexpression in its own
  already-canonicalized form. Introduces brand new locals after
  `IRBuilder` already assigned every other local's own frame offset
  once, which is why `main.cpp` calls `IRBuilder.h`'s own
  `recomputeFrameLayout` right after this pass runs. See
  `LocalCSE.h`'s own comment for the deliberate gaps (no operand-order
  normalization, no iterating to a fixpoint, nothing but `Binary`/
  `Index` ever considered) and the one pre-existing `IRPrinter.cpp`
  cosmetic gap this pass makes far more visible (a promoted
  subexpression's `--dump-ir` text can read ambiguously, though the
  real tree - and so the actual compiled program - is unaffected).

- **`DeadCodeElimination`** (`src/optimizers/dead_code_elimination/DeadCodeElimination.cpp`)
  — removes statements that can never run (anything after a `return`
  in the same block, bare `;` statements) and local variables that
  are declared but never read, written, or had their address taken,
  dropping their storage entirely unless their own initializer has a
  side effect (a call), in which case it's kept as a bare expression
  statement instead.

- **`UnusedGlobalElimination`**
  (`src/optimizers/unused_global_elimination/UnusedGlobalElimination.cpp`)
  — `DeadCodeElimination`'s own local-variable removal, extended to
  globals: drops a global `Slot` nothing reads, writes, or takes the
  address of, directly or (the dispatch-table idiom) through another
  still-live global's own array initializer. Globals need one extra
  safety check locals never do — this subset has no `static`, so a
  global is implicitly external and another, separately compiled file
  could `extern` it — so `main.cpp` only ever runs this pass when
  `!allowExternal` (never under real separate compilation,
  `--format=coff`). Also never removes an explicitly `@addr`-pinned
  global (the pin is itself a statement of intent this pass has no
  way to know is safe to undo) or a `Function`, even an uncalled one
  (see `UnusedGlobalElimination.h`'s own comment on why that's a
  separate, larger feature). Runs last, after `DeadCodeElimination`,
  so a global touched only from now-unreachable code correctly looks
  unused too.

Not yet attempted, because they'd need a real control-flow graph
first: dead-*store* elimination, constant-condition `if`/`while`
removal, and a flow-sensitive (cross-block) CSE.

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
