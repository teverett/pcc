#include "optimizers/dead_code_elimination/DeadCodeElimination.h"

#include <algorithm>
#include <iostream>
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

// LocalDecl's own declSlot field is excluded (a declaration isn't a use);
// any other appearance (read, write, index base, &-taken) counts.
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

// A call or embedded assignment (e.g. `x = (y = 5)`) must survive even once
// the variable it initializes turns out to be dead.
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

// Drops statements after the first `return` in a block and bare `;`
// statements, then recurses. Pass 1 of 2; see run()'s comment on ordering.
void removeUnreachable(ir::Stmt *s, bool &changed, const std::string &sourceFile) {
    if (!s) return;
    switch (s->kind) {
        case ir::StmtKind::Block: {
            for (size_t i = 0; i < s->body.size(); i++) {
                if (s->body[i]->kind == ir::StmtKind::Return && i + 1 < s->body.size()) {
                    std::cerr << sourceFile << ": dce: line " << s->body[i]->line << ": removed "
                               << (s->body.size() - i - 1) << " statement(s) after `return`\n";
                    s->body.resize(i + 1);
                    changed = true;
                    break;
                }
            }
            size_t before = s->body.size();
            s->body.erase(std::remove_if(s->body.begin(), s->body.end(),
                                           [](const ir::StmtPtr &child) { return child->kind == ir::StmtKind::Empty; }),
                            s->body.end());
            if (s->body.size() != before) {
                std::cerr << sourceFile << ": dce: removed " << (before - s->body.size()) << " empty statement(s)\n";
                changed = true;
            }
            for (auto &child : s->body) removeUnreachable(child.get(), changed, sourceFile);
            return;
        }
        case ir::StmtKind::If:
            removeUnreachable(s->thenStmt.get(), changed, sourceFile);
            removeUnreachable(s->elseStmt.get(), changed, sourceFile);
            return;
        case ir::StmtKind::While:
            removeUnreachable(s->loopBody.get(), changed, sourceFile);
            removeUnreachable(s->postStmt.get(), changed, sourceFile);
            return;
        default:
            return;
    }
}

// Pass 2: drops (or reduces to a bare expression statement, if its
// initializer has a side effect) every LocalDecl whose slot isn't in
// `used`, computed after pass 1 so a decl used only from removed
// unreachable code is correctly treated as dead.
void removeDeadDecls(ir::Stmt *s, const std::unordered_set<ir::Slot *> &used, bool &changed,
                       const std::string &sourceFile) {
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
                        std::cerr << sourceFile << ": dce: line " << child->line << ": removed unused local `"
                                   << child->declSlot->sourceName << "`\n";
                        continue;  // drop the declaration entirely
                    }
                    std::cerr << sourceFile << ": dce: line " << child->line << ": local `" << child->declSlot->sourceName
                               << "` is unused but its initializer has side effects; kept as an expression statement\n";
                    child->kind = ir::StmtKind::ExprStmt;
                    child->declSlot = nullptr;
                    newBody.push_back(std::move(child));
                    continue;
                }
                removeDeadDecls(child.get(), used, changed, sourceFile);
                newBody.push_back(std::move(child));
            }
            s->body = std::move(newBody);
            return;
        }
        case ir::StmtKind::If:
            removeDeadDecls(s->thenStmt.get(), used, changed, sourceFile);
            removeDeadDecls(s->elseStmt.get(), used, changed, sourceFile);
            return;
        case ir::StmtKind::While:
            removeDeadDecls(s->loopBody.get(), used, changed, sourceFile);
            removeDeadDecls(s->postStmt.get(), used, changed, sourceFile);
            return;
        default:
            return;
    }
}

}  // namespace

bool DeadCodeElimination::run(ir::Program &prog, const std::string &sourceFile) const {
    bool changed = false;
    for (auto &fb : prog.functionBodies) {
        // Pass 1 must settle reachability before pass 2 collects uses, or a
        // local referenced only from soon-to-be-removed code would wrongly
        // survive.
        removeUnreachable(fb.body.get(), changed, sourceFile);

        std::unordered_set<ir::Slot *> used;
        collectStmtSlots(fb.body.get(), used);
        removeDeadDecls(fb.body.get(), used, changed, sourceFile);
    }
    return changed;
}

}  // namespace pcc
