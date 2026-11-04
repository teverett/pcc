#include "optimizers/unused_global_elimination/UnusedGlobalElimination.h"

#include <algorithm>
#include <memory>
#include <unordered_set>
#include <vector>

namespace pcc {

namespace {

// Every Slot `e` itself names - a read, a write target, an array
// index base, or an &-taken address all count, exactly like
// DeadCodeElimination.cpp's own collectExprSlots (duplicated here
// rather than shared, matching every optimizer in this directory
// being self-contained). Only ever adds a Global Slot to `live` (a
// Local/Param can't be referenced from outside its own function, so
// it's irrelevant to this pass), queuing each newly-discovered one in
// `worklist` so its own array initializer gets followed too.
void collectLiveGlobals(const ir::Expr *e, std::unordered_set<ir::Slot *> &live, std::vector<ir::Slot *> &worklist) {
    if (!e) return;
    if (e->slot && e->slot->kind == ir::SlotKind::Global && live.insert(e->slot).second) {
        worklist.push_back(e->slot);
    }
    collectLiveGlobals(e->lhs.get(), live, worklist);
    collectLiveGlobals(e->rhs.get(), live, worklist);
    for (const auto &arg : e->args) collectLiveGlobals(arg.get(), live, worklist);
}

void collectLiveGlobals(const ir::Stmt *s, std::unordered_set<ir::Slot *> &live, std::vector<ir::Slot *> &worklist) {
    if (!s) return;
    switch (s->kind) {
        case ir::StmtKind::Block:
            for (const auto &child : s->body) collectLiveGlobals(child.get(), live, worklist);
            return;
        case ir::StmtKind::If:
            collectLiveGlobals(s->cond.get(), live, worklist);
            collectLiveGlobals(s->thenStmt.get(), live, worklist);
            collectLiveGlobals(s->elseStmt.get(), live, worklist);
            return;
        case ir::StmtKind::While:
            collectLiveGlobals(s->cond.get(), live, worklist);
            collectLiveGlobals(s->loopBody.get(), live, worklist);
            collectLiveGlobals(s->postStmt.get(), live, worklist);
            return;
        case ir::StmtKind::Return:
        case ir::StmtKind::LocalDecl:
        case ir::StmtKind::ExprStmt:
            collectLiveGlobals(s->initOrReturnOrExpr.get(), live, worklist);
            return;
        case ir::StmtKind::Break:
        case ir::StmtKind::Continue:
        case ir::StmtKind::Empty:
        case ir::StmtKind::Asm:
            return;  // see UnusedGlobalElimination.h's own comment on asm{}'s blind spot
    }
}

}  // namespace

bool UnusedGlobalElimination::run(ir::Program &prog) const {
    std::unordered_set<ir::Slot *> live;
    std::vector<ir::Slot *> worklist;

    for (const auto &fb : prog.functionBodies) collectLiveGlobals(fb.body.get(), live, worklist);

    // Fixpoint: a global reached only through another global's own
    // array initializer is live too, but only once that other global
    // is itself confirmed live - never the other way around (a dead
    // global's own initializer doesn't keep anything else alive).
    while (!worklist.empty()) {
        ir::Slot *g = worklist.back();
        worklist.pop_back();
        for (const auto &elem : g->arrayInit) {
            if (elem.isSymbol && elem.slot && elem.slot->kind == ir::SlotKind::Global && live.insert(elem.slot).second) {
                worklist.push_back(elem.slot);
            }
        }
    }

    bool changed = false;
    prog.slots.erase(std::remove_if(prog.slots.begin(), prog.slots.end(),
                                      [&](const std::unique_ptr<ir::Slot> &slot) {
                                          if (slot->kind != ir::SlotKind::Global) return false;
                                          if (slot->isExternal || slot->hasAddress) return false;
                                          if (live.count(slot.get())) return false;
                                          changed = true;
                                          return true;
                                      }),
                       prog.slots.end());
    return changed;
}

}  // namespace pcc
