#pragma once

#include "optimizers/Optimizer.h"

namespace rcc {

// Removes dead code that needs no control-flow graph or constant folding:
// statements after a `return` in the same block, bare `;` statements, and
// local variables never read/written/address-taken (dropped entirely,
// unless their initializer has a side effect, in which case kept as a bare
// expression statement).
//
// Not attempted (would need a CFG and/or constant folding): dead-store
// elimination, constant-condition if/while removal, CSE.
class DeadCodeElimination : public Optimizer {
   public:
    std::string name() const override { return "dce"; }
    bool run(ir::Program &prog, const std::string &sourceFile) const override;
};

}  // namespace rcc
