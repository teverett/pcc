#pragma once

#include <string>

#include "ir/IR.h"

namespace pcc {

// A single target-independent IR-to-IR transformation, run between
// IRBuilder and a Backend's generate().
class Optimizer {
   public:
    virtual ~Optimizer() = default;

    virtual std::string name() const = 0;

    // Mutates `prog` in place; returns true if anything changed, so a
    // driver re-running passes to a fixpoint knows whether to continue.
    virtual bool run(ir::Program &prog) const = 0;
};

}  // namespace pcc
