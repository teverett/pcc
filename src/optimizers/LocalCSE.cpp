#include "optimizers/LocalCSE.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace pcc {

namespace {

// Same check DeadCodeElimination.cpp's own `hasSideEffect` makes, plus
// `CallPtr` (a call through a computed address - just as side-effecting
// as a direct `Call`, which that helper doesn't otherwise need to tell
// apart from one since it only cares about "any side effect at all").
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

// Only a `Binary`/`Index` is ever considered as its own CSE candidate
// - see LocalCSE.h's own comment for why a bare `Var`/`IntLit`/`Unary`
// isn't. A nested side effect rules out the whole subtree, the same
// conservative bar every other optimizer here uses.
bool isEligible(const ir::Expr *e) {
    if (e->kind != ir::ExprKind::Binary && e->kind != ir::ExprKind::Index) return false;
    return !hasSideEffect(e);
}

// Every `Slot` an eligible (side-effect-free) subtree reads - exactly
// DeadCodeElimination.cpp's own `collectExprSlots` shape, duplicated
// here rather than shared, matching every other optimizer in this
// directory being self-contained. Used only to decide which recorded
// candidates a later write invalidates.
void collectDeps(const ir::Expr *e, std::unordered_set<ir::Slot *> &deps) {
    if (!e) return;
    if (e->slot) deps.insert(e->slot);
    collectDeps(e->lhs.get(), deps);
    collectDeps(e->rhs.get(), deps);
    for (const auto &arg : e->args) collectDeps(arg.get(), deps);
}

// A canonical string for `e`'s own shape - two subtrees get the same
// key iff they're textually identical (same kind/op, same literal
// values, same `Slot`/`Function` identity, recursively) - exactly
// "syntactic (not flow-sensitive) matching" as LocalCSE.h promises:
// no normalization of a commutative operator's operand order, no
// reasoning about two different `Slot`s that might alias. Only ever
// called on an eligible (side-effect-free) subtree, so the four
// side-effecting kinds below can't actually occur inside one; the
// fallback exists only so a bug elsewhere can never cause a false
// match instead of a loud one.
std::string exprKey(const ir::Expr *e) {
    if (!e) return "_";
    switch (e->kind) {
        case ir::ExprKind::IntLit:
            return "I" + std::to_string(e->intValue);
        case ir::ExprKind::Var:
            return "V" + std::to_string(e->slot->id);
        case ir::ExprKind::Index:
            return "X" + std::to_string(e->slot->id) + "[" + exprKey(e->lhs.get()) + "]";
        case ir::ExprKind::Unary:
            return "U" + e->op + "(" + exprKey(e->rhs.get()) + ")";
        case ir::ExprKind::Binary:
            return "B" + e->op + "(" + exprKey(e->lhs.get()) + "," + exprKey(e->rhs.get()) + ")";
        case ir::ExprKind::Func:
            return "F" + std::to_string(e->func->id);
        case ir::ExprKind::Assign:
        case ir::ExprKind::DerefAssign:
        case ir::ExprKind::Call:
        case ir::ExprKind::CallPtr:
        case ir::ExprKind::Intrinsic:
            return "?" + std::to_string(reinterpret_cast<std::uintptr_t>(e));
    }
    return "?" + std::to_string(reinterpret_cast<std::uintptr_t>(e));
}

// Human-readable rendering of an eligible (Binary/Index) subexpression,
// for the console message only - never used for matching (exprKey is
// the canonical key for that).
std::string describeExpr(const ir::Expr *e) {
    if (!e) return "?";
    switch (e->kind) {
        case ir::ExprKind::IntLit:
            return std::to_string(e->intValue);
        case ir::ExprKind::Var:
            return e->slot->sourceName;
        case ir::ExprKind::Index:
            return e->slot->sourceName + "[" + describeExpr(e->lhs.get()) + "]";
        case ir::ExprKind::Unary:
            return e->op + describeExpr(e->rhs.get());
        case ir::ExprKind::Binary:
            return describeExpr(e->lhs.get()) + " " + e->op + " " + describeExpr(e->rhs.get());
        default:
            return "<expr>";
    }
}

ir::ExprPtr makeVarRead(ir::Slot *slot, int line) {
    auto v = std::make_unique<ir::Expr>();
    v->kind = ir::ExprKind::Var;
    v->line = line;
    v->slot = slot;
    return v;
}

// Wraps `original` (moved as-is, unchanged) into `temp = original` -
// still evaluates at exactly `original`'s own program point, still
// produces exactly the same value (an Assign expression's own value
// is the value stored - already relied on by this subset's own
// support for a nested assignment like `x = (y = 5)`), just also
// stores it for every later occurrence to read back out.
ir::ExprPtr makeAssignWrap(ir::ExprPtr original, ir::Slot *temp) {
    auto assign = std::make_unique<ir::Expr>();
    assign->kind = ir::ExprKind::Assign;
    assign->line = original->line;
    assign->slot = temp;
    assign->rhs = std::move(original);
    return assign;
}

ir::Slot *makeTempSlot(ir::Program &prog, int &tempCounter) {
    auto slot = std::make_unique<ir::Slot>();
    slot->id = static_cast<int>(prog.slots.size());  // every Slot id is unique; see IR.h's own comment
    slot->sourceName = "__cse" + std::to_string(tempCounter++);
    slot->kind = ir::SlotKind::Local;
    ir::Slot *raw = slot.get();
    prog.slots.push_back(std::move(slot));
    return raw;
}

// A bare declaration (no initializer) just reserves the temp's own
// frame storage - genStmt's own LocalDecl case stores a zero into it
// right here, immediately overwritten by the real value the very
// first time the wrapped assignment below it runs. Inserted at the
// very front of its own scope's statement list, so it's always well
// before either occurrence - see run()'s own comment for why main.cpp
// has to recompute frame layout after this pass runs.
ir::StmtPtr makeTempDecl(ir::Slot *slot, int line) {
    auto decl = std::make_unique<ir::Stmt>();
    decl->kind = ir::StmtKind::LocalDecl;
    decl->line = line;
    decl->declSlot = slot;
    return decl;
}

// One subexpression recorded so far in the current scope: either
// still waiting for a second sighting (`tempSlot` null, `firstLoc`
// pointing at its one-and-only occurrence so far, ready to be wrapped
// the moment a second one shows up), or already promoted (`tempSlot`
// set - every further sighting just becomes a read of it).
struct Avail {
    ir::ExprPtr *firstLoc = nullptr;
    ir::Slot *tempSlot = nullptr;
    std::unordered_set<ir::Slot *> deps;
};

void invalidateSlot(std::unordered_map<std::string, Avail> &avail, ir::Slot *written) {
    for (auto it = avail.begin(); it != avail.end();) {
        if (it->second.deps.count(written)) {
            it = avail.erase(it);
        } else {
            ++it;
        }
    }
}

void cseExpr(ir::ExprPtr &e, std::unordered_map<std::string, Avail> &avail, std::vector<ir::StmtPtr> &body,
              size_t &i, ir::Program &prog, int &tempCounter, bool &changed) {
    if (!e) return;
    // Bottom-up, same shape every other optimizer here uses - a
    // nested candidate is fully resolved (recorded, matched, or
    // promoted) before its own enclosing expression is even looked
    // at, so the enclosing one's own key always reflects whatever the
    // nested one was just rewritten to (see LocalCSE.h's own comment
    // on the one gap this causes).
    cseExpr(e->lhs, avail, body, i, prog, tempCounter, changed);
    cseExpr(e->rhs, avail, body, i, prog, tempCounter, changed);
    for (auto &arg : e->args) cseExpr(arg, avail, body, i, prog, tempCounter, changed);

    // Apply this node's own invalidating effect - after its children,
    // which (on every backend here) evaluate before it does.
    switch (e->kind) {
        case ir::ExprKind::Assign:
            invalidateSlot(avail, e->slot);
            break;
        case ir::ExprKind::DerefAssign:
        case ir::ExprKind::Call:
        case ir::ExprKind::CallPtr:
        case ir::ExprKind::Intrinsic:
            avail.clear();
            break;
        default:
            break;
    }

    if (!isEligible(e.get())) return;

    std::string key = exprKey(e.get());
    auto it = avail.find(key);
    if (it == avail.end()) {
        std::unordered_set<ir::Slot *> deps;
        collectDeps(e.get(), deps);
        avail.emplace(std::move(key), Avail{&e, nullptr, std::move(deps)});
        return;
    }
    int line = e->line;
    if (it->second.tempSlot) {
        std::cerr << "local-cse: line " << line << ": reused `" << it->second.tempSlot->sourceName
                   << "` instead of recomputing `" << describeExpr(e.get()) << "`\n";
        e = makeVarRead(it->second.tempSlot, line);
        changed = true;
        return;
    }
    // Second sighting - promote now: wrap the first occurrence where
    // it already sits, turn this one into a plain read, and reserve
    // the temp's own storage at the front of this scope.
    std::cerr << "local-cse: line " << line << ": hoisted repeated subexpression `"
               << describeExpr(e.get()) << "` into a temporary\n";
    ir::Slot *temp = makeTempSlot(prog, tempCounter);
    *it->second.firstLoc = makeAssignWrap(std::move(*it->second.firstLoc), temp);
    e = makeVarRead(temp, line);
    body.insert(body.begin(), makeTempDecl(temp, line));
    ++i;  // everything in this scope, including the statement we're still walking, just shifted up by one
    it->second.tempSlot = temp;
    changed = true;
}

void cseVisit(ir::Stmt *s, ir::Program &prog, int &tempCounter, bool &changed);

// The actual CSE engine, scoped to exactly one Block's own statement
// list - see LocalCSE.h's own comment on why a nested If/While body
// never shares this scope's `avail` table in either direction.
void cseBlock(std::vector<ir::StmtPtr> &body, ir::Program &prog, int &tempCounter, bool &changed) {
    std::unordered_map<std::string, Avail> avail;
    for (size_t i = 0; i < body.size(); i++) {
        ir::Stmt *s = body[i].get();
        switch (s->kind) {
            case ir::StmtKind::Block:
                cseBlock(s->body, prog, tempCounter, changed);
                break;
            case ir::StmtKind::If:
                cseExpr(s->cond, avail, body, i, prog, tempCounter, changed);
                cseVisit(s->thenStmt.get(), prog, tempCounter, changed);
                cseVisit(s->elseStmt.get(), prog, tempCounter, changed);
                break;
            case ir::StmtKind::While:
                cseExpr(s->cond, avail, body, i, prog, tempCounter, changed);
                cseVisit(s->loopBody.get(), prog, tempCounter, changed);
                cseVisit(s->postStmt.get(), prog, tempCounter, changed);
                break;
            case ir::StmtKind::Return:
            case ir::StmtKind::LocalDecl:
            case ir::StmtKind::ExprStmt:
                cseExpr(s->initOrReturnOrExpr, avail, body, i, prog, tempCounter, changed);
                break;
            case ir::StmtKind::Break:
            case ir::StmtKind::Continue:
            case ir::StmtKind::Empty:
            case ir::StmtKind::Asm:
                break;
        }
    }
}

// Finds every Block reachable from `s` (each becoming its own fresh
// `cseBlock` scope) without otherwise touching anything - `s` itself,
// when it isn't a Block (a brace-less single-statement If/While
// body), is deliberately left untouched: see LocalCSE.h's own "within
// one Block's own statement list" scoping.
void cseVisit(ir::Stmt *s, ir::Program &prog, int &tempCounter, bool &changed) {
    if (!s) return;
    switch (s->kind) {
        case ir::StmtKind::Block:
            cseBlock(s->body, prog, tempCounter, changed);
            return;
        case ir::StmtKind::If:
            cseVisit(s->thenStmt.get(), prog, tempCounter, changed);
            cseVisit(s->elseStmt.get(), prog, tempCounter, changed);
            return;
        case ir::StmtKind::While:
            cseVisit(s->loopBody.get(), prog, tempCounter, changed);
            cseVisit(s->postStmt.get(), prog, tempCounter, changed);
            return;
        default:
            return;
    }
}

}  // namespace

bool LocalCSE::run(ir::Program &prog) const {
    bool changed = false;
    int tempCounter = 0;
    for (auto &fb : prog.functionBodies) cseVisit(fb.body.get(), prog, tempCounter, changed);
    return changed;
}

}  // namespace pcc
