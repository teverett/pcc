#pragma once

#include "optimizers/Optimizer.h"

namespace pcc {

// Replaces any Unary/Binary expression whose operand(s) are already
// IntLit with the single IntLit its value computes to - e.g. `(2 +
// 3)` becomes `5`, `!(0)` becomes `1`. Purely local to one expression
// node at a time (no data-flow analysis: a Var is never treated as
// constant even if every assignment to it happens to agree), but
// still an unconditional win wherever it applies - fewer runtime
// instructions, and it often exposes new dead code for
// DeadCodeElimination (see ../../README.md's "Optimizers" section)
// to clean up on a later pass, e.g. a LocalDecl initializer that
// folds down to a value never actually read.
//
// Only "-", "!", "~" are folded for Unary - never "*"/"&" (deref/
// address-of), which are never meaningfully applied to a bare
// literal in valid source anyway. A Binary whose fold would need
// host-undefined-behavior to compute (division/modulo by zero, a
// shift amount outside [0, 63]) is deliberately left unfolded - the
// unoptimized code already encodes whatever that target's hardware
// actually does with it, so there's nothing safe to replace it with
// here.
class ConstantFolding : public Optimizer {
   public:
    std::string name() const override { return "constant-folding"; }
    bool run(ir::Program &prog) const override;
};

}  // namespace pcc
