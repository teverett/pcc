#pragma once

#include "optimizers/Optimizer.h"

namespace pcc {

// Simple local common-subexpression elimination / redundant-load
// elimination: purely syntactic (not flow-sensitive) matching of
// identical `Binary`/`Index` subexpressions within one `Block`'s own
// statement list (an `If`/`While`'s own `cond` counts as part of its
// enclosing block; its `then`/`else`/loop body is its own separate,
// unrelated scope - nothing is shared across a block boundary in
// either direction). `Index` is the "redundant load" half of this -
// re-reading `a[i]` costs a real address computation plus a memory
// load on every backend here, often several instructions on a
// register-starved target.
//
// On a second (or later) textually-identical, side-effect-free
// occurrence of the same subexpression within a scope, the *first*
// occurrence is wrapped in place with an assignment into a fresh
// temporary local (so it still evaluates at exactly its original
// program point, in exactly its original position relative to
// anything else - no code is ever moved), and every later occurrence
// becomes a plain read of that temporary instead of recomputing it.
//
// An occurrence stops being "available" to match again (and any
// not-yet-matched recording of it is simply dropped) the moment
// something could have changed its value: a direct assignment to any
// `Slot` the occurrence depends on invalidates only that occurrence,
// while a `Call`/`CallPtr`/`Intrinsic`/`DerefAssign` - none of which
// this pass can reason about the effect of - conservatively
// invalidates every occurrence recorded so far in the scope.
//
// Deliberately narrow, matching its own "simple, syntactic" brief:
// never reorders anything (so there's no risk of a subexpression's
// hoisted computation running at a different point, relative to a
// side effect, than the source did); only considers `Binary`/`Index`
// (a bare `Var`/`IntLit` read is already as cheap as the temporary
// read that would replace it, so there's nothing to gain there; a
// `Unary` is a single cheap instruction on every backend here, not
// worth a temporary's own store); matches `a + b` against `a + b` but
// never against `b + a` even for a commutative operator (true textual
// matching only, no operand-order normalization); and - because a
// subexpression nested inside a larger one is always resolved (left
// alone, or promoted to a temporary) before the larger one is even
// looked at - a composite expression that itself recurs verbatim
// (e.g. `(a[i] + 1) * (a[i] + 1)`) only ever gets its *inner* `a[i]`
// shared, never the outer `... + 1` as well, once the inner one has
// already been rewritten into a temporary read on one side but not
// read identically on both (a real but accepted gap for a pass this
// simple - a second run would catch it, but this pass doesn't iterate
// to a fixpoint).
// A promoted expression's `--dump-ir` text can look ambiguous - e.g.
// `(x + tmp = y)` for what's actually `((tmp = y) + tmp)` - because
// IRPrinter.cpp's own Assign case doesn't parenthesize itself the way
// Binary's own does; the grammar has no generic "(expr)" alternative
// to print it with even if it tried (every operator's rule supplies
// its own required parens, including Assign's own, which has none).
// Cosmetic only: codegen and IRValidator's own self-check both work
// from the real tree, never the printed text, so this never affects
// what actually gets compiled - just what a human reads back from a
// dump.
class LocalCSE : public Optimizer {
   public:
    std::string name() const override { return "local-cse"; }
    bool run(ir::Program &prog) const override;
};

}  // namespace pcc
