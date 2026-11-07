#pragma once

#include "optimizers/Optimizer.h"

namespace pcc {

// Strength reduction: rewrites a Binary expression with exactly one IntLit
// operand into a cheaper identity (e.g. `x * 2` -> `x << 1`, `x / 1` -> `x`).
// Never drops the non-literal operand, so a side effect in it still runs
// exactly once.
//
// Deliberately not rewritten: `x * 0`/`x % 1`/`x & 0`, since reaching the
// simplified value would require dropping the non-literal operand, unsafe
// if it has a side effect (unanalyzed here). Also not rewritten: `x /` or
// `% <power of two>` as shift/mask, since truncating division isn't
// bit-identical to a right-shift for negative x, and sign isn't known here.
class AlgebraicSimplification : public Optimizer {
   public:
    std::string name() const override { return "algebraic-simplification"; }
    bool run(ir::Program &prog) const override;
};

}  // namespace pcc
