#pragma once

#include "optimizers/Optimizer.h"

namespace pcc {

// Removes the cheapest, always-safe forms of dead code from every
// function body - no control-flow graph and no constant folding
// required (see ../../README.md's "Optimizers" section):
//
//  - statements that can never run: anything after a `return` in the
//    same block, and bare `;` (Empty) statements.
//  - local variables that are declared but never read, written, or
//    had their address taken anywhere in the function - the
//    declaration is dropped entirely (so neither backend emits
//    storage or a label for it), unless its own initializer has a
//    side effect (a call), in which case the declaration is kept as
//    a bare expression statement so that side effect still happens,
//    just without the now-pointless storage.
//
// Deliberately NOT attempted here (would need a real CFG and/or
// constant folding first): dead-*store* elimination (a write to a
// variable that IS read later, just not after this particular
// write), constant-condition if/while removal, and CSE.
class DeadCodeElimination : public Optimizer {
   public:
    std::string name() const override { return "dce"; }
    bool run(ir::Program &prog) const override;
};

}  // namespace pcc
