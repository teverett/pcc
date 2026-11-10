#pragma once

#include "optimizers/Optimizer.h"

namespace rcc {

// Removes a store to a scalar Local/Param whose value is never used -
// closing the gap ConstantPropagation.h's own header documents. Restricted,
// like ConstantPropagation, to a candidate whose address is never taken
// anywhere in its function (a pre-pass scans for `&slot` first): nothing
// else can alias it, so only a direct Assign/CompoundAssign naming that
// same slot, or an unparsed `asm {}` block, needs to be tracked at all.
//
// Two independent, non-fixpoint cases:
//
// 1. Write-only: a candidate that's written one or more times but never
//    read anywhere in its function has every one of those stores dropped.
//    A plain `slot = expr;` becomes empty (or, if `expr` has a side
//    effect, just `expr;`); a LocalDecl's own initializer is cleared the
//    same way, but only when side-effect-free - dropping just the
//    initializer while keeping the declaration (still needed for a later
//    write) would otherwise need splitting it into two statements, which
//    this pass doesn't do; a side-effecting write-only LocalDecl
//    initializer is left in place.
//
// 2. Immediate shadowing: within one straight-line run of statements (one
//    Block's own list; see LocalCSE.h's identical scoping choice), a plain
//    `slot = expr;` immediately followed - with no intervening read of
//    `slot` - by another store to the same slot is dead and is dropped the
//    same way as case 1. Entering an If/While clears all tracked stores
//    before recursing into each branch/body with its own fresh, empty
//    state, and the state stays empty for the rest of that scan afterward
//    too - simple and safe, at the cost of never tracking a store across a
//    branch, even when every branch provably overwrites it. A LocalDecl's
//    own initializer is never tracked as the earlier, droppable store in
//    this case (only case 1 ever removes one), so one immediately shadowed
//    by a later store, with `slot` still read somewhere else in the
//    function, is a known gap this pass doesn't close.
class DeadStoreElimination : public Optimizer {
   public:
    std::string name() const override { return "dead-store-elimination"; }
    bool run(ir::Program &prog, const std::string &sourceFile) const override;
};

}  // namespace rcc
