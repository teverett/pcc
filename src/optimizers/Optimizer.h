#pragma once

#include <string>

#include "ir/IR.h"

namespace pcc {

// A single IR-to-IR transformation, run (see main.cpp's --optimize)
// between IRBuilder (ir/IRBuilder.h) and a Backend's own generate()
// (codegen/Codegen.h) - entirely target-independent, since by the
// time the IR exists every backend-specific decision (instruction
// selection, register/stack strategy) still lies ahead; see
// optimizers/dead_code_elimination/DeadCodeElimination.h for the
// first one. Each concrete optimizer lives in its own subdirectory
// under optimizers/, mirroring how each codegen backend lives in its
// own subdirectory under codegen/ (see Codegen.h).
class Optimizer {
   public:
    virtual ~Optimizer() = default;

    // Short identifying name, for diagnostics/logging - not currently
    // surfaced anywhere, but kept for parity with Backend::name().
    virtual std::string name() const = 0;

    // Mutates `prog` in place. Returns true if anything in it
    // actually changed, so a driver running several optimizers in
    // sequence (or re-running one to a fixpoint) knows whether
    // another pass might still find more to do.
    virtual bool run(ir::Program &prog) const = 0;
};

}  // namespace pcc
