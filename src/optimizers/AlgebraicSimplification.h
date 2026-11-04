#pragma once

#include "optimizers/Optimizer.h"

namespace pcc {

// Rewrites a Binary expression with exactly one IntLit operand into a
// cheaper-to-execute equivalent, when one exists that's true for
// every possible value of the other (non-literal) operand - e.g.
// `x * 2` becomes `x << 1` (same bit pattern on any two's-complement
// machine, so this holds regardless of what backend-specific
// overflow/wraparound behavior `x`'s own real width has), `x / 1`
// becomes `x`. This is strength reduction, not constant folding (see
// ConstantFolding.h, which already handles the case where BOTH
// operands are already IntLit) - it only ever replaces the
// *operation*, never drops the non-literal operand's own
// subexpression, so a side effect inside it (a call) still runs
// exactly once either way.
//
// Deliberately NOT rewritten here, since each would need to drop the
// non-literal operand entirely to reach the "simplified" value, which
// is only safe if that operand has no side effect (an analysis this
// pass doesn't do - see DeadCodeElimination.cpp's own `hasSideEffect`
// for the one place in this project that does): `x * 0` (-> 0), `x %
// 1` (-> 0), `x & 0` (-> 0). Also not rewritten: `x / <power of two>`
// / `x % <power of two>` as a shift/mask - unlike multiplication,
// truncating (round-toward-zero) division isn't bit-identical to an
// arithmetic right-shift for a negative `x`, and this pass has no way
// to know `x`'s sign at compile time in general.
class AlgebraicSimplification : public Optimizer {
   public:
    std::string name() const override { return "algebraic-simplification"; }
    bool run(ir::Program &prog) const override;
};

}  // namespace pcc
