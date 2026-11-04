#include "optimizers/DeadCodeElimination.h"

#include <algorithm>
#include <unordered_set>

namespace pcc {

namespace {

void collectExprSlots(const ir::Expr *e, std::unordered_set<ir::Slot *> &used) {
    if (!e) return;
    if (e->slot) used.insert(e->slot);
    collectExprSlots(e->lhs.get(), used);
    collectExprSlots(e->rhs.get(), used);
    for (const auto &arg : e->args) collectExprSlots(arg.get(), used);
}

// Only LocalDecl's own `declSlot` field is excluded (a declaration by
// itself isn't a use) - every other appearance of a Slot, in any
// Expr anywhere in the function, counts: a read, a write (Assign's
// target), an array index base, or an `&`-taken address all make a
// local's storage observably necessary.
void collectStmtSlots(const ir::Stmt *s, std::unordered_set<ir::Slot *> &used) {
    if (!s) return;
    switch (s->kind) {
        case ir::StmtKind::Block:
            for (const auto &child : s->body) collectStmtSlots(child.get(), used);
            return;
        case ir::StmtKind::If:
            collectExprSlots(s->cond.get(), used);
            collectStmtSlots(s->thenStmt.get(), used);
            collectStmtSlots(s->elseStmt.get(), used);
            return;
        case ir::StmtKind::While:
            collectExprSlots(s->cond.get(), used);
            collectStmtSlots(s->loopBody.get(), used);
            collectStmtSlots(s->postStmt.get(), used);
            return;
        case ir::StmtKind::Return:
        case ir::StmtKind::LocalDecl:
        case ir::StmtKind::ExprStmt:
            collectExprSlots(s->initOrReturnOrExpr.get(), used);
            return;
        case ir::StmtKind::Break:
        case ir::StmtKind::Continue:
        case ir::StmtKind::Empty:
        case ir::StmtKind::Asm:
            return;
    }
}

// Whether evaluating `e` can do anything beyond compute a value - a
// call (this function has no idea what the callee does) or an
// embedded assignment (the grammar allows assignment as an
// expression, e.g. `x = (y = 5)`). If so, a LocalDecl initializer
// built from it has to survive even once the variable it initializes
// turns out to be dead.
bool hasSideEffect(const ir::Expr *e) {
    if (!e) return false;
    switch (e->kind) {
        case ir::ExprKind::Call:
        case ir::ExprKind::Intrinsic:
        case ir::ExprKind::Assign:
        case ir::ExprKind::DerefAssign:
            return true;
        default:
            break;
    }
    if (hasSideEffect(e->lhs.get())) return true;
    if (hasSideEffect(e->rhs.get())) return true;
    for (const auto &arg : e->args) {
        if (hasSideEffect(arg.get())) return true;
    }
    return false;
}

// Drops every statement after the first `return` within the same
// block (it can never run) and every bare `;` (Empty) statement, then
// recurses into whatever block structure remains. Pass 1 of 2 - see
// DeadCodeElimination::run()'s own comment for why this has to finish
// completely before pass 2 (removeDeadDecls) starts.
void removeUnreachable(ir::Stmt *s, bool &changed) {
    if (!s) return;
    switch (s->kind) {
        case ir::StmtKind::Block: {
            for (size_t i = 0; i < s->body.size(); i++) {
                if (s->body[i]->kind == ir::StmtKind::Return && i + 1 < s->body.size()) {
                    s->body.resize(i + 1);
                    changed = true;
                    break;
                }
            }
            size_t before = s->body.size();
            s->body.erase(std::remove_if(s->body.begin(), s->body.end(),
                                           [](const ir::StmtPtr &child) { return child->kind == ir::StmtKind::Empty; }),
                            s->body.end());
            if (s->body.size() != before) changed = true;
            for (auto &child : s->body) removeUnreachable(child.get(), changed);
            return;
        }
        case ir::StmtKind::If:
            removeUnreachable(s->thenStmt.get(), changed);
            removeUnreachable(s->elseStmt.get(), changed);
            return;
        case ir::StmtKind::While:
            removeUnreachable(s->loopBody.get(), changed);
            removeUnreachable(s->postStmt.get(), changed);
            return;
        default:
            return;
    }
}

// Pass 2: drops (or neuters down to a bare expression statement, if
// its initializer has a side effect) every LocalDecl whose slot isn't
// in `used` - computed from the tree as pass 1 left it, so a decl
// whose only "use" was itself inside now-removed unreachable code is
// correctly treated as dead too.
void removeDeadDecls(ir::Stmt *s, const std::unordered_set<ir::Slot *> &used, bool &changed) {
    if (!s) return;
    switch (s->kind) {
        case ir::StmtKind::Block: {
            std::vector<ir::StmtPtr> newBody;
            newBody.reserve(s->body.size());
            for (auto &child : s->body) {
                if (child->kind == ir::StmtKind::LocalDecl && used.find(child->declSlot) == used.end()) {
                    changed = true;
                    bool isArray = child->declSlot->arraySize > 0;
                    if (isArray || !child->initOrReturnOrExpr || !hasSideEffect(child->initOrReturnOrExpr.get())) {
                        continue;  // drop the declaration entirely
                    }
                    child->kind = ir::StmtKind::ExprStmt;
                    child->declSlot = nullptr;
                    newBody.push_back(std::move(child));
                    continue;
                }
                removeDeadDecls(child.get(), used, changed);
                newBody.push_back(std::move(child));
            }
            s->body = std::move(newBody);
            return;
        }
        case ir::StmtKind::If:
            removeDeadDecls(s->thenStmt.get(), used, changed);
            removeDeadDecls(s->elseStmt.get(), used, changed);
            return;
        case ir::StmtKind::While:
            removeDeadDecls(s->loopBody.get(), used, changed);
            removeDeadDecls(s->postStmt.get(), used, changed);
            return;
        default:
            return;
    }
}

}  // namespace

bool DeadCodeElimination::run(ir::Program &prog) const {
    bool changed = false;
    for (auto &fb : prog.functionBodies) {
        // Pass 1 must fully settle which statements are even reachable
        // before pass 2 decides which locals are used - otherwise a
        // local referenced only from code pass 1 is about to delete
        // would wrongly look "used" and survive.
        removeUnreachable(fb.body.get(), changed);

        std::unordered_set<ir::Slot *> used;
        collectStmtSlots(fb.body.get(), used);
        removeDeadDecls(fb.body.get(), used, changed);
    }
    return changed;
}

}  // namespace pcc
