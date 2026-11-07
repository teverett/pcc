#include "optimizers/unused_global_elimination/UnusedGlobalElimination.h"

#include <algorithm>
#include <memory>
#include <unordered_set>
#include <vector>

namespace pcc {

namespace {

// Only adds a Global Slot to `live` (Local/Param can't be referenced
// outside their own function), queuing each newly-discovered one so its
// own array initializer gets followed too.
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
            return;  // asm{} blind spot; see UnusedGlobalElimination.h
    }
}

}  // namespace

bool UnusedGlobalElimination::run(ir::Program &prog, const std::string &) const {
    std::unordered_set<ir::Slot *> live;
    std::vector<ir::Slot *> worklist;

    for (const auto &fb : prog.functionBodies) collectLiveGlobals(fb.body.get(), live, worklist);

    // Fixpoint: a global reached through another global's array
    // initializer is live only once that other global is confirmed live.
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
