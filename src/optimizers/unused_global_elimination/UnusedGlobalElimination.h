#pragma once

#include "optimizers/Optimizer.h"

namespace pcc {

// Removes every global Slot that nothing in the program reads, writes, or
// takes the address of, directly or via another live global's array
// initializer (the dispatch-table idiom). Since this subset has no
// static/visibility keyword, a global is implicitly external and another
// translation unit could extern it, so this pass is only safe under
// !allowExternal (whole-program compiles) — main.cpp must not run it under
// --format=coff.
//
// Also excluded: a global with an explicit @addr pin (its own statement of
// intent), and a global touched only from an unparsed asm {} block (not
// scanned for symbol references, same blind spot as DeadCodeElimination's
// local removal).
//
// Only Slots are removed, never Functions: a function's address can escape
// via &name in ways this pass doesn't trace.
class UnusedGlobalElimination : public Optimizer {
   public:
    std::string name() const override { return "unused-global-elimination"; }
    bool run(ir::Program &prog, const std::string &sourceFile) const override;
};

}  // namespace pcc
