#include "optimizers/constant_propagation/ConstantPropagation.h"

#include <iostream>
#include <unordered_map>
#include <unordered_set>

namespace rcc {

namespace {

bool isCandidateSlot(ir::Slot *slot) {
    return slot != nullptr && slot->arraySize == 0 &&
           (slot->kind == ir::SlotKind::Local || slot->kind == ir::SlotKind::Param);
}

// --- Pre-pass: every scalar whose address escapes anywhere in the body --

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

// --- Loop helpers: what a While's own body/post could write, and whether
// an unparsed Asm inside it means "could write anything" instead ---------

bool containsAsm(const ir::Stmt *s) {
    if (!s) return false;
    switch (s->kind) {
        case ir::StmtKind::Block:
            for (const auto &child : s->body) {
                if (containsAsm(child.get())) return true;
            }
            return false;
        case ir::StmtKind::If:
            return containsAsm(s->thenStmt.get()) || containsAsm(s->elseStmt.get());
        case ir::StmtKind::While:
            return containsAsm(s->loopBody.get()) || containsAsm(s->postStmt.get());
        case ir::StmtKind::Asm:
            return true;
        default:
            return false;
    }
}

void collectWrittenSlots(const ir::Expr *e, std::unordered_set<ir::Slot *> &written) {
    if (!e) return;
    if ((e->kind == ir::ExprKind::Assign || e->kind == ir::ExprKind::CompoundAssign) && !e->lhs && e->slot) {
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
            if (s->declSlot) written.insert(s->declSlot);
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

// --- The propagation itself ----------------------------------------------

ir::ExprPtr makeIntLit(long value, int line, Type type) {
    auto lit = std::make_unique<ir::Expr>();
    lit->kind = ir::ExprKind::IntLit;
    lit->line = line;
    lit->intValue = value;
    lit->type = type;
    return lit;
}

using KnownMap = std::unordered_map<ir::Slot *, long>;

// Kept after an If only where both branches agree - either on the value
// itself, or (via a copy of the pre-If map) on "unchanged".
KnownMap intersectKnown(const KnownMap &a, const KnownMap &b) {
    KnownMap result;
    for (const auto &[slot, value] : a) {
        auto it = b.find(slot);
        if (it != b.end() && it->second == value) result.emplace(slot, value);
    }
    return result;
}

void propagateStmt(ir::Stmt *s, KnownMap &known, const std::unordered_set<ir::Slot *> &addressTaken, bool &changed,
                     const std::string &sourceFile);

// Bottom-up, same shape as ConstantFolding/LocalCSE's own expression
// walks; a nested Assign/CompoundAssign updates `known` on the way back
// up, after its own rhs has already had a chance to become a literal.
void propagateExpr(ir::ExprPtr &e, KnownMap &known, const std::unordered_set<ir::Slot *> &addressTaken,
                     bool &changed, const std::string &sourceFile) {
    if (!e) return;

    // `&operand` needs operand's real lvalue, never a cached value copy -
    // same restriction, and same reason, as LocalCSE's own skip for this
    // case. An Index's own subscript (e.g. i in &arr[i]) still gets the
    // normal treatment below.
    if (e->kind == ir::ExprKind::Unary && e->op == "&") {
        if (e->rhs && e->rhs->kind == ir::ExprKind::Index) {
            propagateExpr(e->rhs->lhs, known, addressTaken, changed, sourceFile);
        }
        return;
    }

    propagateExpr(e->lhs, known, addressTaken, changed, sourceFile);
    propagateExpr(e->rhs, known, addressTaken, changed, sourceFile);
    propagateExpr(e->elseExpr, known, addressTaken, changed, sourceFile);
    for (auto &arg : e->args) propagateExpr(arg, known, addressTaken, changed, sourceFile);

    if (e->kind == ir::ExprKind::Var && e->slot) {
        auto it = known.find(e->slot);
        if (it != known.end()) {
            std::cerr << sourceFile << ": constant-propagation: line " << e->line << ": replaced `"
                       << e->slot->sourceName << "` with its known value `" << it->second << "`\n";
            e = makeIntLit(it->second, e->line, e->type);
            changed = true;
        }
        return;
    }

    // `e->lhs` set on either of these means an indexed/array-element
    // target (`slot[lhs] = rhs`), not a whole-scalar overwrite of `slot` -
    // already excluded from `known` by isCandidateSlot, but checked
    // explicitly too since it's the IR's own documented distinction (see
    // ExprKind::Assign/CompoundAssign in IR.h).
    if (e->kind == ir::ExprKind::Assign && !e->lhs && e->slot && isCandidateSlot(e->slot) &&
        !addressTaken.count(e->slot)) {
        if (e->rhs && e->rhs->kind == ir::ExprKind::IntLit) {
            known[e->slot] = e->rhs->intValue;
        } else {
            known.erase(e->slot);
        }
    } else if (e->kind == ir::ExprKind::CompoundAssign && !e->lhs && e->slot && isCandidateSlot(e->slot) &&
                !addressTaken.count(e->slot)) {
        known.erase(e->slot);
    }
}

void propagateStmt(ir::Stmt *s, KnownMap &known, const std::unordered_set<ir::Slot *> &addressTaken, bool &changed,
                     const std::string &sourceFile) {
    if (!s) return;
    switch (s->kind) {
        case ir::StmtKind::Block:
            for (auto &child : s->body) propagateStmt(child.get(), known, addressTaken, changed, sourceFile);
            return;
        case ir::StmtKind::If: {
            propagateExpr(s->cond, known, addressTaken, changed, sourceFile);
            KnownMap thenKnown = known;
            propagateStmt(s->thenStmt.get(), thenKnown, addressTaken, changed, sourceFile);
            KnownMap elseKnown = known;
            propagateStmt(s->elseStmt.get(), elseKnown, addressTaken, changed, sourceFile);
            known = intersectKnown(thenKnown, elseKnown);
            return;
        }
        case ir::StmtKind::While: {
            // `cond` is the same AST node re-evaluated on every iteration,
            // not just the first - it must be walked with any slot the
            // loop itself (re)writes already dropped from `known`, or a
            // substitution valid only on entry gets baked into the
            // condition forever (e.g. `i <= 10` permanently rewritten to
            // `1 <= 10` from `i`'s pre-loop value, looping forever).
            if (containsAsm(s->loopBody.get()) || containsAsm(s->postStmt.get())) {
                known.clear();
                propagateExpr(s->cond, known, addressTaken, changed, sourceFile);
                propagateStmt(s->loopBody.get(), known, addressTaken, changed, sourceFile);
                propagateStmt(s->postStmt.get(), known, addressTaken, changed, sourceFile);
                known.clear();
                return;
            }
            std::unordered_set<ir::Slot *> written;
            collectWrittenSlots(s->cond.get(), written);
            collectWrittenSlots(s->loopBody.get(), written);
            collectWrittenSlots(s->postStmt.get(), written);
            for (ir::Slot *slot : written) known.erase(slot);
            propagateExpr(s->cond, known, addressTaken, changed, sourceFile);
            propagateStmt(s->loopBody.get(), known, addressTaken, changed, sourceFile);
            propagateStmt(s->postStmt.get(), known, addressTaken, changed, sourceFile);
            // A value (re)learned inside the loop only held for its last
            // iteration, not necessarily on exit after zero iterations -
            // drop it again rather than letting it leak past the loop.
            for (ir::Slot *slot : written) known.erase(slot);
            return;
        }
        case ir::StmtKind::Return:
        case ir::StmtKind::ExprStmt:
            propagateExpr(s->initOrReturnOrExpr, known, addressTaken, changed, sourceFile);
            return;
        case ir::StmtKind::LocalDecl:
            propagateExpr(s->initOrReturnOrExpr, known, addressTaken, changed, sourceFile);
            if (s->declSlot && isCandidateSlot(s->declSlot) && !addressTaken.count(s->declSlot)) {
                if (s->initOrReturnOrExpr && s->initOrReturnOrExpr->kind == ir::ExprKind::IntLit) {
                    known[s->declSlot] = s->initOrReturnOrExpr->intValue;
                } else {
                    known.erase(s->declSlot);
                }
            }
            return;
        case ir::StmtKind::Asm:
            known.clear();
            return;
        case ir::StmtKind::Break:
        case ir::StmtKind::Continue:
        case ir::StmtKind::Empty:
            return;
        // Only `switch` ever produces these (see IR.h); a Label is reached
        // both by falling through from whatever precedes it here AND by a
        // jump from one of the switch's own comparisons (which runs with
        // `known` exactly as it stood on switch entry, since none of the
        // comparisons it jumps from mutate anything) - so, same as an If's
        // two branches, `known` can't just carry through unconditionally.
        // Clearing is the conservative sound choice; a Goto needs no
        // handling of its own; it's always immediately followed by a Label.
        case ir::StmtKind::Label:
            known.clear();
            return;
        case ir::StmtKind::Goto:
            return;
    }
}

}  // namespace

bool ConstantPropagation::run(ir::Program &prog, const std::string &sourceFile) const {
    bool changed = false;
    for (auto &fb : prog.functionBodies) {
        std::unordered_set<ir::Slot *> addressTaken;
        collectAddressTaken(fb.body.get(), addressTaken);
        KnownMap known;
        propagateStmt(fb.body.get(), known, addressTaken, changed, sourceFile);
    }
    return changed;
}

}  // namespace rcc
