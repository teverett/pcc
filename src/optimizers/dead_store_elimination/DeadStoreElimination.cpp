#include "optimizers/dead_store_elimination/DeadStoreElimination.h"

#include <iostream>
#include <unordered_map>
#include <unordered_set>

namespace rcc {

namespace {

bool isCandidateSlot(ir::Slot *slot) {
    return slot != nullptr && slot->arraySize == 0 &&
           (slot->kind == ir::SlotKind::Local || slot->kind == ir::SlotKind::Param);
}

// Same as ConstantPropagation.cpp's own collectAddressTaken.
void collectAddressTaken(const ir::Expr *e, std::unordered_set<ir::Slot *> &addressTaken) {
    if (!e) return;
    if (e->kind == ir::ExprKind::Unary && e->op == "&" && e->rhs && e->rhs->kind == ir::ExprKind::Var) {
        addressTaken.insert(e->rhs->slot);
    }
    collectAddressTaken(e->lhs.get(), addressTaken);
    collectAddressTaken(e->rhs.get(), addressTaken);
    collectAddressTaken(e->elseExpr.get(), addressTaken);
    for (const auto &arg : e->args) collectAddressTaken(arg.get(), addressTaken);
}

void collectAddressTaken(const ir::Stmt *s, std::unordered_set<ir::Slot *> &addressTaken) {
    if (!s) return;
    switch (s->kind) {
        case ir::StmtKind::Block:
            for (const auto &child : s->body) collectAddressTaken(child.get(), addressTaken);
            return;
        case ir::StmtKind::If:
            collectAddressTaken(s->cond.get(), addressTaken);
            collectAddressTaken(s->thenStmt.get(), addressTaken);
            collectAddressTaken(s->elseStmt.get(), addressTaken);
            return;
        case ir::StmtKind::While:
            collectAddressTaken(s->cond.get(), addressTaken);
            collectAddressTaken(s->loopBody.get(), addressTaken);
            collectAddressTaken(s->postStmt.get(), addressTaken);
            return;
        case ir::StmtKind::Return:
        case ir::StmtKind::LocalDecl:
        case ir::StmtKind::ExprStmt:
            collectAddressTaken(s->initOrReturnOrExpr.get(), addressTaken);
            return;
        case ir::StmtKind::Break:
        case ir::StmtKind::Continue:
        case ir::StmtKind::Empty:
        case ir::StmtKind::Asm:
        case ir::StmtKind::Label:
        case ir::StmtKind::Goto:
            return;
    }
}

// Same as DeadCodeElimination.cpp's own hasSideEffect, minus CallPtr (never
// a candidate's rhs top-level kind, but harmless either way via recursion).
bool hasSideEffect(const ir::Expr *e) {
    if (!e) return false;
    switch (e->kind) {
        case ir::ExprKind::Call:
        case ir::ExprKind::CallPtr:
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

// Converts an ExprStmt holding `slot = rhs;` into either Empty (rhs has no
// side effect) or a bare `rhs;` (side effect kept, the store itself dropped).
void neutralizeAssignStmt(ir::Stmt *exprStmt) {
    ir::ExprPtr assignExpr = std::move(exprStmt->initOrReturnOrExpr);
    if (hasSideEffect(assignExpr->rhs.get())) {
        exprStmt->initOrReturnOrExpr = std::move(assignExpr->rhs);
    } else {
        exprStmt->kind = ir::StmtKind::Empty;
        exprStmt->initOrReturnOrExpr = nullptr;
    }
}

// --- Case 1: immediate shadowing within one straight-line run -----------

using Pending = std::unordered_map<ir::Slot *, ir::Stmt *>;  // slot -> its still-live store, not yet read

// A real read of `slot` (a Var, or a CompoundAssign's own implicit read of
// its target's current value) consumes any pending store for it; nothing
// else can, since every candidate's address is provably un-taken (see
// DeadStoreElimination.h and ConstantPropagation.h's identical reasoning).
void visitExprReads(const ir::Expr *e, Pending &pending) {
    if (!e) return;
    if (e->kind == ir::ExprKind::Unary && e->op == "&") {
        if (e->rhs && e->rhs->kind == ir::ExprKind::Index) visitExprReads(e->rhs->lhs.get(), pending);
        return;
    }
    visitExprReads(e->lhs.get(), pending);
    visitExprReads(e->rhs.get(), pending);
    visitExprReads(e->elseExpr.get(), pending);
    for (const auto &arg : e->args) visitExprReads(arg.get(), pending);

    if (e->kind == ir::ExprKind::Var && e->slot) {
        pending.erase(e->slot);
    } else if (e->kind == ir::ExprKind::CompoundAssign && !e->lhs && e->slot) {
        pending.erase(e->slot);
    }
}

void scanStmt(ir::Stmt *s, Pending &pending, const std::unordered_set<ir::Slot *> &addressTaken, bool &changed,
               const std::string &sourceFile) {
    if (!s) return;
    switch (s->kind) {
        case ir::StmtKind::Block:
            for (auto &child : s->body) scanStmt(child.get(), pending, addressTaken, changed, sourceFile);
            return;
        case ir::StmtKind::If: {
            visitExprReads(s->cond.get(), pending);
            // Too hard to reason about what either branch did once they've
            // run, so every store tracked so far - before or after this
            // statement, in this same scan - is given up rather than risk
            // dropping one some path still needs.
            pending.clear();
            Pending thenPending;
            scanStmt(s->thenStmt.get(), thenPending, addressTaken, changed, sourceFile);
            Pending elsePending;
            scanStmt(s->elseStmt.get(), elsePending, addressTaken, changed, sourceFile);
            return;
        }
        case ir::StmtKind::While: {
            visitExprReads(s->cond.get(), pending);
            pending.clear();
            Pending bodyPending;
            scanStmt(s->loopBody.get(), bodyPending, addressTaken, changed, sourceFile);
            Pending postPending;
            scanStmt(s->postStmt.get(), postPending, addressTaken, changed, sourceFile);
            return;
        }
        case ir::StmtKind::Return:
        case ir::StmtKind::LocalDecl:
            // A LocalDecl's own initializer is never registered as a
            // pending store below (see DeadStoreElimination.h's gap note),
            // only scanned for the reads it contains.
            visitExprReads(s->initOrReturnOrExpr.get(), pending);
            return;
        case ir::StmtKind::ExprStmt: {
            ir::Expr *e = s->initOrReturnOrExpr.get();
            if (e && e->kind == ir::ExprKind::Assign && !e->lhs && e->slot && isCandidateSlot(e->slot) &&
                !addressTaken.count(e->slot)) {
                visitExprReads(e->rhs.get(), pending);  // rhs evaluates before the store
                auto it = pending.find(e->slot);
                if (it != pending.end()) {
                    std::cerr << sourceFile << ": dead-store-elimination: line " << it->second->line
                               << ": dropped store to `" << e->slot->sourceName
                               << "`, overwritten at line " << s->line << " with no read in between\n";
                    neutralizeAssignStmt(it->second);
                    changed = true;
                }
                pending[e->slot] = s;
            } else {
                visitExprReads(e, pending);
            }
            return;
        }
        case ir::StmtKind::Break:
        case ir::StmtKind::Continue:
        case ir::StmtKind::Empty:
            return;
        case ir::StmtKind::Asm:
            pending.clear();  // raw text, not parsed for symbol references
            return;
        // Only `switch` produces these (see IR.h). A Label is reachable
        // both by falling through from whatever precedes it here and by a
        // jump from one of the switch's own comparisons (which runs with
        // `pending` exactly as it stood on switch entry) - same reasoning
        // as ConstantPropagation.cpp's own Label case, so clearing is the
        // only sound choice. Goto needs nothing of its own.
        case ir::StmtKind::Label:
            pending.clear();
            return;
        case ir::StmtKind::Goto:
            return;
    }
}

// --- Case 2: write-only, whole function -----------------------------------

void collectReadSlots(const ir::Expr *e, std::unordered_set<ir::Slot *> &read) {
    if (!e) return;
    if (e->kind == ir::ExprKind::Unary && e->op == "&") {
        if (e->rhs && e->rhs->kind == ir::ExprKind::Index) collectReadSlots(e->rhs->lhs.get(), read);
        return;
    }
    collectReadSlots(e->lhs.get(), read);
    collectReadSlots(e->rhs.get(), read);
    collectReadSlots(e->elseExpr.get(), read);
    for (const auto &arg : e->args) collectReadSlots(arg.get(), read);

    if (e->kind == ir::ExprKind::Var && e->slot && isCandidateSlot(e->slot)) {
        read.insert(e->slot);
    } else if (e->kind == ir::ExprKind::CompoundAssign && !e->lhs && e->slot && isCandidateSlot(e->slot)) {
        read.insert(e->slot);
    }
}

void collectReadSlots(const ir::Stmt *s, std::unordered_set<ir::Slot *> &read) {
    if (!s) return;
    switch (s->kind) {
        case ir::StmtKind::Block:
            for (const auto &child : s->body) collectReadSlots(child.get(), read);
            return;
        case ir::StmtKind::If:
            collectReadSlots(s->cond.get(), read);
            collectReadSlots(s->thenStmt.get(), read);
            collectReadSlots(s->elseStmt.get(), read);
            return;
        case ir::StmtKind::While:
            collectReadSlots(s->cond.get(), read);
            collectReadSlots(s->loopBody.get(), read);
            collectReadSlots(s->postStmt.get(), read);
            return;
        case ir::StmtKind::Return:
        case ir::StmtKind::LocalDecl:
        case ir::StmtKind::ExprStmt:
            collectReadSlots(s->initOrReturnOrExpr.get(), read);
            return;
        case ir::StmtKind::Break:
        case ir::StmtKind::Continue:
        case ir::StmtKind::Empty:
        case ir::StmtKind::Asm:
        case ir::StmtKind::Label:
        case ir::StmtKind::Goto:
            return;
    }
}

void collectWrittenSlots(const ir::Expr *e, std::unordered_set<ir::Slot *> &written) {
    if (!e) return;
    if ((e->kind == ir::ExprKind::Assign || e->kind == ir::ExprKind::CompoundAssign) && !e->lhs && e->slot &&
        isCandidateSlot(e->slot)) {
        written.insert(e->slot);
    }
    collectWrittenSlots(e->lhs.get(), written);
    collectWrittenSlots(e->rhs.get(), written);
    collectWrittenSlots(e->elseExpr.get(), written);
    for (const auto &arg : e->args) collectWrittenSlots(arg.get(), written);
}

void collectWrittenSlots(const ir::Stmt *s, std::unordered_set<ir::Slot *> &written) {
    if (!s) return;
    switch (s->kind) {
        case ir::StmtKind::Block:
            for (const auto &child : s->body) collectWrittenSlots(child.get(), written);
            return;
        case ir::StmtKind::If:
            collectWrittenSlots(s->cond.get(), written);
            collectWrittenSlots(s->thenStmt.get(), written);
            collectWrittenSlots(s->elseStmt.get(), written);
            return;
        case ir::StmtKind::While:
            collectWrittenSlots(s->cond.get(), written);
            collectWrittenSlots(s->loopBody.get(), written);
            collectWrittenSlots(s->postStmt.get(), written);
            return;
        case ir::StmtKind::Return:
        case ir::StmtKind::ExprStmt:
            collectWrittenSlots(s->initOrReturnOrExpr.get(), written);
            return;
        case ir::StmtKind::LocalDecl:
            if (s->declSlot && isCandidateSlot(s->declSlot)) written.insert(s->declSlot);
            collectWrittenSlots(s->initOrReturnOrExpr.get(), written);
            return;
        case ir::StmtKind::Break:
        case ir::StmtKind::Continue:
        case ir::StmtKind::Empty:
        case ir::StmtKind::Asm:
        case ir::StmtKind::Label:
        case ir::StmtKind::Goto:
            return;
    }
}

void removeWriteOnlyStores(ir::Stmt *s, const std::unordered_set<ir::Slot *> &writeOnly, bool &changed,
                             const std::string &sourceFile) {
    if (!s) return;
    switch (s->kind) {
        case ir::StmtKind::Block:
            for (auto &child : s->body) removeWriteOnlyStores(child.get(), writeOnly, changed, sourceFile);
            return;
        case ir::StmtKind::If:
            removeWriteOnlyStores(s->thenStmt.get(), writeOnly, changed, sourceFile);
            removeWriteOnlyStores(s->elseStmt.get(), writeOnly, changed, sourceFile);
            return;
        case ir::StmtKind::While:
            removeWriteOnlyStores(s->loopBody.get(), writeOnly, changed, sourceFile);
            removeWriteOnlyStores(s->postStmt.get(), writeOnly, changed, sourceFile);
            return;
        case ir::StmtKind::LocalDecl:
            if (s->declSlot && writeOnly.count(s->declSlot) && s->initOrReturnOrExpr &&
                !hasSideEffect(s->initOrReturnOrExpr.get())) {
                std::cerr << sourceFile << ": dead-store-elimination: line " << s->line << ": dropped `"
                           << s->declSlot->sourceName << "`'s own never-read initializer\n";
                s->initOrReturnOrExpr = nullptr;
                changed = true;
            }
            // A side-effecting write-only initializer is left in place -
            // see DeadStoreElimination.h's own gap note.
            return;
        case ir::StmtKind::ExprStmt: {
            ir::Expr *e = s->initOrReturnOrExpr.get();
            if (e && e->kind == ir::ExprKind::Assign && !e->lhs && e->slot && writeOnly.count(e->slot)) {
                std::cerr << sourceFile << ": dead-store-elimination: line " << s->line
                           << ": dropped never-read store to `" << e->slot->sourceName << "`\n";
                neutralizeAssignStmt(s);
                changed = true;
            }
            return;
        }
        case ir::StmtKind::Return:
        case ir::StmtKind::Break:
        case ir::StmtKind::Continue:
        case ir::StmtKind::Empty:
        case ir::StmtKind::Asm:
        case ir::StmtKind::Label:
        case ir::StmtKind::Goto:
            return;
    }
}

}  // namespace

bool DeadStoreElimination::run(ir::Program &prog, const std::string &sourceFile) const {
    bool changed = false;
    for (auto &fb : prog.functionBodies) {
        std::unordered_set<ir::Slot *> addressTaken;
        collectAddressTaken(fb.body.get(), addressTaken);

        Pending pending;
        scanStmt(fb.body.get(), pending, addressTaken, changed, sourceFile);

        std::unordered_set<ir::Slot *> written;
        std::unordered_set<ir::Slot *> read;
        collectWrittenSlots(fb.body.get(), written);
        collectReadSlots(fb.body.get(), read);
        std::unordered_set<ir::Slot *> writeOnly;
        for (ir::Slot *slot : written) {
            if (!read.count(slot) && !addressTaken.count(slot)) writeOnly.insert(slot);
        }
        if (!writeOnly.empty()) removeWriteOnlyStores(fb.body.get(), writeOnly, changed, sourceFile);
    }
    return changed;
}

}  // namespace rcc
