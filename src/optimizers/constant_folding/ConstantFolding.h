#pragma once

#include "optimizers/Optimizer.h"

namespace rcc {

// Replaces a Unary/Binary expression whose operand(s) are already IntLit
// with the IntLit it computes to (e.g. `2 + 3` -> `5`). Purely local to one
// node (no data-flow analysis; a Var is never treated as constant).
//
// A Binary fold needing host-undefined-behavior (division/modulo by zero, a
// shift outside [0, 63]) is left unfolded, since the real target's hardware
// has its own well-defined answer that this pass shouldn't override.
class ConstantFolding : public Optimizer {
   public:
    std::string name() const override { return "constant-folding"; }
    bool run(ir::Program &prog, const std::string &sourceFile) const override;
};

}  // namespace rcc
