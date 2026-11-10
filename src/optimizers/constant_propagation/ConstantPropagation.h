#pragma once

#include "optimizers/Optimizer.h"

namespace rcc {

// Intraprocedural constant propagation: walking one function's body in
// source order, tracks which Local/Param scalars currently hold a known
// literal value and replaces a later read of one with that literal
// directly - feeding ConstantFolding more foldable expressions, and
// DeadCodeElimination more now-unused declarations, than either pass
// finds on its own.
//
// Restricted to scalars (arraySize == 0) whose address is never taken
// anywhere in the function (a pre-pass scans the whole body for `&slot`
// first). That restriction is what keeps the rest of the pass simple:
// since nothing else - not a Call, not a DerefAssign/CompoundDerefAssign
// through some other pointer - can ever alias a tracked slot, only a
// direct Assign/CompoundAssign naming that same slot can invalidate its
// known value; there's no need to conservatively drop everything at a
// Call the way LocalCSE does for its own (unrestricted) candidates.
//
// Flow through an If merges the two branches' knowledge afterward: a slot
// keeps its known value only when both branches agree on it (a missing
// else is treated as "nothing changed" on that path). Flow through a
// While conservatively forgets, both before and after the loop, every
// slot the loop body or post-statement could write - correct for zero or
// more iterations without running an actual fixpoint. An Asm statement is
// a blind spot (its raw text isn't parsed), so it clears every known
// value outright, the same gap UnusedGlobalElimination's own header
// already accepts for a different reason.
//
// Known gap: doesn't do dead-store elimination. Once every read of a
// local is gone, DeadCodeElimination still drops a now-unused LocalDecl,
// but a plain `slot = <expr>;` ExprStmt whose value is never read again
// is left in place - no existing pass removes a dead store.
class ConstantPropagation : public Optimizer {
   public:
    std::string name() const override { return "constant-propagation"; }
    bool run(ir::Program &prog, const std::string &sourceFile) const override;
};

}  // namespace rcc
