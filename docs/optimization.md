# Optimizers

`--optimize` runs a pipeline of target-independent IR-to-IR passes
(`src/optimizers/`, each implementing the `Optimizer` interface in
`src/optimizers/Optimizer.h`) between `IRBuilder` and whichever
backend is selected — since every backend-specific decision
(instruction selection, register/stack strategy) happens strictly
after the IR exists, an optimizer written once benefits every target
(see `docs/platforms.md` for the full list).

Today there are nine, run in this order (`src/main.cpp`). Eight implement
the shared `Optimizer` interface; `AddressHoisting` is the exception (see
below).

- **`ConstantPropagation`**
  (`src/optimizers/constant_propagation/ConstantPropagation.cpp`) —
  intraprocedural: walking one function body in source order, tracks
  which scalar Local/Param currently holds a known literal and
  replaces a later read with it directly, feeding `ConstantFolding`
  new foldable expressions and `DeadCodeElimination` more unused
  declarations. Restricted to a scalar whose address is never taken
  (so only a direct write to it can invalidate the tracked value, with
  no need to conservatively drop everything at a `Call`). An `If`
  keeps a value only when both branches agree on it; a `While`
  conservatively forgets every slot the loop could write, both before
  and after. Runs first, feeding every later pass.

- **`ConstantFolding`** (`src/optimizers/constant_folding/ConstantFolding.cpp`) —
  bottom-up folds any unary or binary expression whose operand(s) are
  already `IntLit`s into a single `IntLit` (e.g. `((2 + 3) * 4)` →
  `20`). Leaves an op unfolded when evaluating it on the host would
  require undefined behavior the real target's own instruction
  handles fine instead — division/modulo by a constant zero, or a
  shift by an out-of-range amount.

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
  `recomputeFrameLayout` once the later passes below are done. See
  `LocalCSE.h`'s own comment for the deliberate gaps (no operand-order
  normalization, no iterating to a fixpoint, nothing but `Binary`/
  `Index` ever considered) and the one pre-existing `IRPrinter.cpp`
  cosmetic gap this pass makes far more visible (a promoted
  subexpression's `--dump-ir` text can read ambiguously, though the
  real tree - and so the actual compiled program - is unaffected).

- **`AddressHoisting`** (`src/optimizers/address_hoisting/AddressHoisting.cpp`)
  — hoists the base address of a real array (a Local/Global `Slot` with
  `arraySize > 0`) out of every repeated `Index`/`Assign`/
  `CompoundAssign` that names it within one function, into a single
  pointer-typed temporary computed once, instead of every backend
  re-deriving that address from scratch at each use. Deliberately
  conservative: a real array's storage address never changes during one
  function activation, so every occurrence naming the same array `Slot`
  is hoisted unconditionally once 2+ occurrences exist, with no
  invalidation analysis needed. Needs a target's `TypeWidths` (to scale
  the index by the array's element width), so unlike the other passes
  this doesn't implement the `Optimizer` interface — `main.cpp` calls it
  directly, like `recomputeFrameLayout`. Runs after `LocalCSE` for the
  same canonicalization reason the passes below do.

- **`DeadStoreElimination`**
  (`src/optimizers/dead_store_elimination/DeadStoreElimination.cpp`) —
  removes a store to a scalar Local/Param whose value is never read
  again, closing the gap `ConstantPropagation` leaves open. Same
  address-never-taken restriction. Two cases, neither run to a
  fixpoint: a candidate written but never read has every store to it
  dropped; within one straight-line run of statements, a plain
  `slot = expr;` immediately followed (no intervening read) by another
  store to the same slot is dropped too. Runs before
  `DeadCodeElimination` so a `LocalDecl` it strips down to a bare,
  unread declaration looks unused to that pass too.

- **`DeadCodeElimination`** (`src/optimizers/dead_code_elimination/DeadCodeElimination.cpp`)
  — removes statements that can never run (anything after a `return`
  in the same block, bare `;` statements) and local variables that
  are declared but never read, written, or had their address taken,
  dropping their storage entirely unless their own initializer has a
  side effect (a call), in which case it's kept as a bare expression
  statement instead.

- **`IdenticalCodeFolding`**
  (`src/optimizers/identical_code_folding/IdenticalCodeFolding.cpp`) —
  merges functions with structurally identical bodies (up to
  alpha-renaming of Params/Locals) into one definition, redirecting
  every `Call`/`&fn` reference to a kept survivor. Changes which
  function a merged pair's `&fn` resolves to, so code comparing two
  originally-distinct function pointers for equality can observe the
  merge — the same tradeoff other compilers accept under `-Os`. Only
  a function with a body and no `@addr` pin participates, and only
  when `!allowExternal`: a separately compiled file might still
  reference a losing definition by name.

- **`UnusedGlobalElimination`**
  (`src/optimizers/unused_global_elimination/UnusedGlobalElimination.cpp`)
  — `DeadCodeElimination`'s own local-variable removal, extended to
  globals: drops a global `Slot` nothing reads, writes, or takes the
  address of, directly or (the dispatch-table idiom) through another
  still-live global's own array initializer. Same `!allowExternal`
  restriction as `IdenticalCodeFolding` — this subset has no `static`,
  so a global is implicitly external and another, separately compiled
  file could `extern` it. Also never removes an explicitly
  `@addr`-pinned global or a `Function`, even an uncalled one. Runs
  last, after `DeadCodeElimination`, so a global touched only from
  now-unreachable code correctly looks unused too.

Not yet attempted, because it'd need a real control-flow graph:
constant-condition `if`/`while` removal, and a flow-sensitive
(cross-block) CSE.

See `docs/ir.md` for the IR these passes operate on, its printed-text
grammar, and `IRValidator`'s self-check.
