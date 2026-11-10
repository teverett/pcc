#include "optimizers/identical_code_folding/IdenticalCodeFolding.h"

#include <algorithm>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace pcc {

namespace {

// Tracks the function currently being canonicalized (for self-recursion)
// and assigns each of its own Locals a stable index on first sighting, so
// two structurally identical bodies produce the same key regardless of
// their Slots'/Functions' real ids.
struct CanonCtx {
    const ir::Function *self;
    std::unordered_map<const ir::Slot *, int> localIndex;
    int nextLocal = 0;
};

std::string typeKey(const Type &t) {
    return std::to_string(static_cast<int>(t.base)) + (t.isUnsigned ? "u" : "s") + "^" + std::to_string(t.ptrDepth);
}

std::string canonSlot(const ir::Slot *slot, CanonCtx &ctx) {
    if (!slot) return "_";
    switch (slot->kind) {
        case ir::SlotKind::Param:
            return "P" + std::to_string(slot->paramIndex);
        case ir::SlotKind::Global:
            return "G" + std::to_string(slot->id);
        case ir::SlotKind::Local: {
            auto it = ctx.localIndex.find(slot);
            if (it != ctx.localIndex.end()) return "L" + std::to_string(it->second);
            int idx = ctx.nextLocal++;
            ctx.localIndex.emplace(slot, idx);
            return "L" + std::to_string(idx);
        }
    }
    return "?";
}

// A call/address-of to the function being canonicalized itself matches the
// same in the other body even though the two Function pointers differ;
// any other callee must be the literal same Function.
std::string canonFunc(const ir::Function *func, const CanonCtx &ctx) {
    if (!func) return "_";
    if (func == ctx.self) return "SELF";
    return "FN" + std::to_string(func->id);
}

std::string canonArrayInit(const std::vector<ir::ArrayInitElem> &init, CanonCtx &ctx) {
    std::string out = "{";
    for (const auto &elem : init) {
        out += elem.isSymbol ? (elem.slot ? "s" + canonSlot(elem.slot, ctx) : "f" + canonFunc(elem.func, ctx))
                              : "v" + std::to_string(elem.value);
        out += ",";
    }
    return out + "}";
}

std::string canonExpr(const ir::Expr *e, CanonCtx &ctx) {
    if (!e) return "_";
    std::string out;
    switch (e->kind) {
        case ir::ExprKind::IntLit:
            out = "I" + std::to_string(e->intValue);
            break;
        case ir::ExprKind::Var:
            out = "V" + canonSlot(e->slot, ctx);
            break;
        case ir::ExprKind::Index:
            out = "X" + canonSlot(e->slot, ctx) + "[" + canonExpr(e->lhs.get(), ctx) + "]";
            break;
        case ir::ExprKind::Assign:
            out = "A" + canonSlot(e->slot, ctx) + (e->lhs ? "[" + canonExpr(e->lhs.get(), ctx) + "]" : "") + "=" +
                  canonExpr(e->rhs.get(), ctx);
            break;
        case ir::ExprKind::DerefAssign:
            out = "D=" + canonExpr(e->lhs.get(), ctx) + ":" + canonExpr(e->rhs.get(), ctx);
            break;
        case ir::ExprKind::CompoundAssign:
            out = "CA" + e->op + (e->yieldOld ? "p" : "") + canonSlot(e->slot, ctx) +
                  (e->lhs ? "[" + canonExpr(e->lhs.get(), ctx) + "]" : "") + "," + canonExpr(e->rhs.get(), ctx);
            break;
        case ir::ExprKind::CompoundDerefAssign:
            out = "CD" + e->op + (e->yieldOld ? "p" : "") + canonExpr(e->lhs.get(), ctx) + "," +
                  canonExpr(e->rhs.get(), ctx);
            break;
        case ir::ExprKind::Unary:
            out = "U" + e->op + "(" + canonExpr(e->rhs.get(), ctx) + ")";
            break;
        case ir::ExprKind::Binary:
            out = "B" + e->op + "(" + canonExpr(e->lhs.get(), ctx) + "," + canonExpr(e->rhs.get(), ctx) + ")";
            break;
        case ir::ExprKind::Call: {
            out = "C" + canonFunc(e->func, ctx) + "(";
            for (const auto &a : e->args) out += canonExpr(a.get(), ctx) + ",";
            out += ")";
            break;
        }
        case ir::ExprKind::Func:
            out = "F" + canonFunc(e->func, ctx);
            break;
        case ir::ExprKind::CallPtr:
            out = "CP(" + canonExpr(e->lhs.get(), ctx) + ")";
            break;
        case ir::ExprKind::Intrinsic:
            out = "N" + e->op;
            break;
        case ir::ExprKind::Convert:
            out = "CV(" + canonExpr(e->lhs.get(), ctx) + ")";
            break;
        case ir::ExprKind::Ternary:
            out = "T(" + canonExpr(e->lhs.get(), ctx) + "?" + canonExpr(e->rhs.get(), ctx) + ":" +
                  canonExpr(e->elseExpr.get(), ctx) + ")";
            break;
    }
    return out + "#" + typeKey(e->type);
}

std::string canonStmt(const ir::Stmt *s, CanonCtx &ctx) {
    if (!s) return "_";
    switch (s->kind) {
        case ir::StmtKind::Block: {
            std::string out = "{";
            for (const auto &c : s->body) out += canonStmt(c.get(), ctx) + ";";
            return out + "}";
        }
        case ir::StmtKind::If:
            return "if(" + canonExpr(s->cond.get(), ctx) + ")" + canonStmt(s->thenStmt.get(), ctx) + "else" +
                   canonStmt(s->elseStmt.get(), ctx);
        case ir::StmtKind::While:
            return "while(" + canonExpr(s->cond.get(), ctx) + ")" + canonStmt(s->loopBody.get(), ctx) + "post" +
                   canonStmt(s->postStmt.get(), ctx);
        case ir::StmtKind::Return:
            return "return " + canonExpr(s->initOrReturnOrExpr.get(), ctx);
        case ir::StmtKind::Break:
            return "break";
        case ir::StmtKind::Continue:
            return "continue";
        case ir::StmtKind::LocalDecl: {
            const ir::Slot *d = s->declSlot;
            std::string out = "decl" + canonSlot(d, ctx) + "#" + typeKey(d->type) + "#" +
                               std::to_string(d->arraySize) + "#" +
                               (d->hasInit ? "i" + std::to_string(d->initValue) : "n") + "#" +
                               canonArrayInit(d->arrayInit, ctx);
            if (s->initOrReturnOrExpr) out += "=" + canonExpr(s->initOrReturnOrExpr.get(), ctx);
            return out;
        }
        case ir::StmtKind::ExprStmt:
            return "expr " + canonExpr(s->initOrReturnOrExpr.get(), ctx);
        case ir::StmtKind::Empty:
            return "empty";
        case ir::StmtKind::Asm:
            return "asm[" + s->asmText + "]";
        // Only `switch` produces these (see IR.h). labelId is a global
        // counter, not reset per function, so two structurally-identical
        // switches in different functions will always carry different ids
        // here - this conservatively stops them from folding rather than
        // risking a false match; it never folds two bodies that aren't
        // truly identical.
        case ir::StmtKind::Label:
            return "label" + std::to_string(s->labelId);
        case ir::StmtKind::Goto:
            return "goto" + std::to_string(s->labelId);
    }
    return "?";
}

std::string signatureOf(const ir::Function *f, const ir::Stmt *body, CanonCtx &ctx) {
    std::string sig = "R" + typeKey(f->returnType) + "|N" + std::to_string(f->params.size());
    for (const ir::Slot *p : f->params) sig += "|" + typeKey(p->type);
    return sig + "||" + canonStmt(body, ctx);
}

using Redirect = std::unordered_map<ir::Function *, ir::Function *>;

void redirectExpr(ir::Expr *e, const Redirect &redirect) {
    if (!e) return;
    if ((e->kind == ir::ExprKind::Call || e->kind == ir::ExprKind::Func) && e->func) {
        auto it = redirect.find(e->func);
        if (it != redirect.end()) e->func = it->second;
    }
    redirectExpr(e->lhs.get(), redirect);
    redirectExpr(e->rhs.get(), redirect);
    redirectExpr(e->elseExpr.get(), redirect);
    for (auto &arg : e->args) redirectExpr(arg.get(), redirect);
}

void redirectStmt(ir::Stmt *s, const Redirect &redirect) {
    if (!s) return;
    redirectExpr(s->cond.get(), redirect);
    redirectExpr(s->initOrReturnOrExpr.get(), redirect);
    for (auto &c : s->body) redirectStmt(c.get(), redirect);
    redirectStmt(s->thenStmt.get(), redirect);
    redirectStmt(s->elseStmt.get(), redirect);
    redirectStmt(s->loopBody.get(), redirect);
    redirectStmt(s->postStmt.get(), redirect);
}

void redirectArrayInit(std::vector<ir::ArrayInitElem> &init, const Redirect &redirect) {
    for (auto &elem : init) {
        if (!elem.isSymbol || !elem.func) continue;
        auto it = redirect.find(elem.func);
        if (it != redirect.end()) elem.func = it->second;
    }
}

}  // namespace

bool IdenticalCodeFolding::run(ir::Program &prog, const std::string &sourceFile) const {
    std::unordered_map<std::string, std::vector<size_t>> groups;
    for (size_t i = 0; i < prog.functionBodies.size(); i++) {
        ir::Function *f = prog.functionBodies[i].func;
        if (f->isExternal || f->hasAddress) continue;
        CanonCtx ctx{f, {}, 0};
        groups[signatureOf(f, prog.functionBodies[i].body.get(), ctx)].push_back(i);
    }

    Redirect redirect;
    std::unordered_set<ir::Function *> losers;
    for (auto &[sig, idxs] : groups) {
        if (idxs.size() < 2) continue;
        // `main`'s own definition can't be dropped: every backend finds it
        // by scanning prog.functions for that name, independent of any Call
        // node this pass tracks.
        size_t keeperPos = 0;
        for (size_t k = 0; k < idxs.size(); k++) {
            if (prog.functionBodies[idxs[k]].func->sourceName == "main") {
                keeperPos = k;
                break;
            }
        }
        ir::Function *keeper = prog.functionBodies[idxs[keeperPos]].func;
        for (size_t k = 0; k < idxs.size(); k++) {
            if (k == keeperPos) continue;
            ir::Function *loser = prog.functionBodies[idxs[k]].func;
            redirect[loser] = keeper;
            losers.insert(loser);
            std::cerr << sourceFile << ": identical-code-folding: merged `" << loser->sourceName << "` into `"
                       << keeper->sourceName << "` (identical body)\n";
        }
    }

    if (redirect.empty()) return false;

    for (auto &fb : prog.functionBodies) redirectStmt(fb.body.get(), redirect);
    for (auto &slot : prog.slots) redirectArrayInit(slot->arrayInit, redirect);

    prog.functionBodies.erase(std::remove_if(prog.functionBodies.begin(), prog.functionBodies.end(),
                                                [&](const ir::FunctionBody &fb) { return losers.count(fb.func) != 0; }),
                                prog.functionBodies.end());
    prog.functions.erase(std::remove_if(prog.functions.begin(), prog.functions.end(),
                                          [&](const std::unique_ptr<ir::Function> &f) {
                                              return losers.count(f.get()) != 0;
                                          }),
                           prog.functions.end());
    return true;
}

}  // namespace pcc
