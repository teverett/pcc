#pragma once

#include "optimizers/Optimizer.h"

namespace pcc {

// Merges functions with structurally identical bodies into one definition,
// redirecting every Call/`&fn` reference to a kept survivor and dropping the
// rest - a code-size win with no runtime cost, independent of target.
//
// Two bodies match if they're identical up to alpha-renaming of their own
// Params (by position) and Locals (by first-occurrence order); a Global or
// another Function referenced from both must be the literal same one (a
// self-recursive call matches a self-recursive call in the other body, even
// though the two Function pointers differ). Only functions with a body,
// without an `@addr` pin, participate - a pinned function keeps its own
// distinct address, and only whole-program compiles (`!allowExternal`) know
// every reference well enough to safely drop the losing definitions (same
// restriction as UnusedGlobalElimination).
//
// Known gap: this changes which function a merged pair's address (`&fn`)
// resolves to - code that compares two originally-distinct function
// pointers for equality can observe the merge, same tradeoff other
// compilers' identical-code-folding passes accept under -Os.
class IdenticalCodeFolding : public Optimizer {
   public:
    std::string name() const override { return "identical-code-folding"; }
    bool run(ir::Program &prog, const std::string &sourceFile) const override;
};

}  // namespace pcc
