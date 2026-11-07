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
    // `sourceFile` is used only to prefix this pass's own diagnostic
    // output, so callers that don't care about diagnostics may pass "".
    virtual bool run(ir::Program &prog, const std::string &sourceFile) const = 0;
};

}  // namespace pcc
