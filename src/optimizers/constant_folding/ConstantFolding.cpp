#include "optimizers/constant_folding/ConstantFolding.h"

#include <iostream>
#include <optional>
#include <string>

namespace pcc {

namespace {

ir::ExprPtr makeIntLit(long value, int line, Type type) {
    auto lit = std::make_unique<ir::Expr>();
    lit->kind = ir::ExprKind::IntLit;
    lit->line = line;
    lit->intValue = value;
    lit->type = type;
    return lit;
}

std::optional<long> evalUnary(const std::string &op, long a) {
    if (op == "-") return -a;
    if (op == "!") return a == 0 ? 1 : 0;
    if (op == "~") return ~a;
    return std::nullopt;  // "*"/"&" (deref/address-of) - never foldable
}

// nullopt means "don't fold this" - either the op isn't one of ours,
// or folding it here would require host-undefined-behavior (a
// division/modulo by a constant zero, or a shift by an amount a
// native `long` shift can't give a well-defined answer for) that the
// real target's own runtime instruction has a perfectly well-defined
// (if silly) answer for instead.
std::optional<long> evalBinary(const std::string &op, long a, long b) {
    if (op == "+") return a + b;
    if (op == "-") return a - b;
    if (op == "*") return a * b;
    if (op == "/") return b != 0 ? std::optional<long>(a / b) : std::nullopt;
    if (op == "%") return b != 0 ? std::optional<long>(a % b) : std::nullopt;
    if (op == "<<") return (b >= 0 && b < 64) ? std::optional<long>(a << b) : std::nullopt;
    if (op == ">>") return (b >= 0 && b < 64) ? std::optional<long>(a >> b) : std::nullopt;
    if (op == "==") return a == b ? 1 : 0;
    if (op == "!=") return a != b ? 1 : 0;
    if (op == "<") return a < b ? 1 : 0;
    if (op == "<=") return a <= b ? 1 : 0;
    if (op == ">") return a > b ? 1 : 0;
    if (op == ">=") return a >= b ? 1 : 0;
    if (op == "&") return a & b;
    if (op == "^") return a ^ b;
    if (op == "|") return a | b;
    if (op == "&&") return (a != 0 && b != 0) ? 1 : 0;
    if (op == "||") return (a != 0 || b != 0) ? 1 : 0;
    return std::nullopt;
}

// Folds `e`'s own children first (so e.g. `((2 + 3) * 4)` folds
// bottom-up into `20`, not left half-folded), then tries to fold `e`
// itself in place. Every other ExprKind (Var/Index/Assign/
// DerefAssign/Call/Intrinsic) is left alone - not constant by
// definition, though its own children may have just been folded.
void foldExpr(ir::ExprPtr &e, bool &changed) {
    if (!e) return;
    foldExpr(e->lhs, changed);
    foldExpr(e->rhs, changed);
    for (auto &arg : e->args) foldExpr(arg, changed);

    if (e->kind == ir::ExprKind::Unary && e->rhs->kind == ir::ExprKind::IntLit) {
        if (std::optional<long> v = evalUnary(e->op, e->rhs->intValue)) {
            std::cerr << "constant-folding: line " << e->line << ": folded `" << e->op
                       << e->rhs->intValue << "` to `" << *v << "`\n";
            e = makeIntLit(*v, e->line, e->type);
            changed = true;
        }
    } else if (e->kind == ir::ExprKind::Binary && e->lhs->kind == ir::ExprKind::IntLit &&
               e->rhs->kind == ir::ExprKind::IntLit) {
        if (std::optional<long> v = evalBinary(e->op, e->lhs->intValue, e->rhs->intValue)) {
            std::cerr << "constant-folding: line " << e->line << ": folded `" << e->lhs->intValue
                       << " " << e->op << " " << e->rhs->intValue << "` to `" << *v << "`\n";
            e = makeIntLit(*v, e->line, e->type);
            changed = true;
        }
    }
}

void foldStmt(ir::Stmt *s, bool &changed) {
    if (!s) return;
    switch (s->kind) {
        case ir::StmtKind::Block:
            for (auto &child : s->body) foldStmt(child.get(), changed);
            return;
        case ir::StmtKind::If:
            foldExpr(s->cond, changed);
            foldStmt(s->thenStmt.get(), changed);
            foldStmt(s->elseStmt.get(), changed);
            return;
        case ir::StmtKind::While:
            foldExpr(s->cond, changed);
            foldStmt(s->loopBody.get(), changed);
            foldStmt(s->postStmt.get(), changed);
            return;
        case ir::StmtKind::Return:
        case ir::StmtKind::LocalDecl:
        case ir::StmtKind::ExprStmt:
            foldExpr(s->initOrReturnOrExpr, changed);
            return;
        case ir::StmtKind::Break:
        case ir::StmtKind::Continue:
        case ir::StmtKind::Empty:
        case ir::StmtKind::Asm:
            return;
    }
}

}  // namespace

bool ConstantFolding::run(ir::Program &prog) const {
    bool changed = false;
    for (auto &fb : prog.functionBodies) foldStmt(fb.body.get(), changed);
    return changed;
}

}  // namespace pcc
