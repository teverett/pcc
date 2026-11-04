#pragma once

#include "optimizers/Optimizer.h"

namespace pcc {

// Removes every global `Slot` (`Program::slots`, `SlotKind::Global`)
// that nothing in the program ever reads, writes, or takes the
// address of - directly, or indirectly through another still-live
// global's own array initializer (the dispatch-table idiom, e.g.
// `int *t[] = {&g, ...}`, keeps `g` alive too). This is
// `DeadCodeElimination`'s own local-variable removal, extended to
// globals - except a global needs one extra safety check a local
// never does: this subset has no `static`/visibility keyword at all,
// so a global is implicitly external, and another, separately
// compiled file could `extern` it even though nothing in *this* file
// still references it. Safe only when this compile sees the entire
// program, so `main.cpp` only ever runs this pass when
// `!allowExternal` (never under real separate compilation,
// `--format=coff`'s own per-file `isExternal` resolution - see IR.h's
// own comment on `Slot::isExternal`); calling it any other way can
// silently remove a global another translation unit still needs.
//
// Two more deliberately conservative exclusions, neither of which
// `DeadCodeElimination`'s own local removal has to worry about: a
// global with an explicit `@addr` pin from source is never removed
// even if otherwise unreferenced (the pin is itself a statement of
// intent - reserving that exact memory location - this pass has no
// way to know is safe to undo); and, like every optimizer in this
// directory, nothing inside an `asm { ... }` block's own raw,
// unparsed text is ever scanned for a symbol reference, so a global
// touched only from one is at real risk of being removed out from
// under it (the same blind spot `DeadCodeElimination.cpp`'s own
// `collectStmtSlots` already has for locals - not new here).
//
// A `Function` is never removed by this pass, only `Slot`s - trimming
// unused functions is a different, larger feature (removing one could
// still be observably wrong even in a whole-program compile, since
// this subset's own `&name` idiom can make a function's address
// escape in ways this pass doesn't trace) that nothing has asked for
// yet.
class UnusedGlobalElimination : public Optimizer {
   public:
    std::string name() const override { return "unused-global-elimination"; }
    bool run(ir::Program &prog) const override;
};

}  // namespace pcc
