#include "optimizers/algebraic_simplification/AlgebraicSimplification.h"

#include <iostream>
#include <optional>
#include <string>

namespace pcc {

namespace {

ir::ExprPtr makeIntLit(long value, int line) {
    auto lit = std::make_unique<ir::Expr>();
    lit->kind = ir::ExprKind::IntLit;
    lit->line = line;
    lit->intValue = value;
    return lit;
}

ir::ExprPtr makeNegate(ir::ExprPtr operand, int line) {
    auto neg = std::make_unique<ir::Expr>();
    neg->kind = ir::ExprKind::Unary;
    neg->op = "-";
    neg->line = line;
    neg->rhs = std::move(operand);
    return neg;
}

ir::ExprPtr makeShift(const std::string &op, ir::ExprPtr operand, long amount, int line) {
    auto shift = std::make_unique<ir::Expr>();
    shift->kind = ir::ExprKind::Binary;
    shift->op = op;
    shift->line = line;
    shift->lhs = std::move(operand);
    shift->rhs = makeIntLit(amount, line);
    return shift;
}

// n's own log2, or nullopt if n isn't a positive power of two - the
// only shape where `x * n` and `x << log2(n)` compute the identical
// bit pattern for every possible value of x.
std::optional<long> log2OfPowerOfTwo(long n) {
    if (n <= 0 || (n & (n - 1)) != 0) return std::nullopt;
    long k = 0;
    while (n > 1) {
        n >>= 1;
        ++k;
    }
    return k;
}

// `op` is `e`'s own operator, with exactly one of lhs/rhs an IntLit
// (both-literal is ConstantFolding's job, run separately - see
// AlgebraicSimplification.h) and `litVal` that literal's value.
// Returns true if an identity applies - for a non-commutative op
// (`/`, `-`, `<<`, `>>`) the literal must be on the rhs for any of
// these to hold, which `literalWasLhs` lets the caller check first.
bool hasIdentityRewrite(const std::string &op, long litVal, bool literalWasLhs) {
    // "*"/"+"/"|"/"^" are commutative, so their identities hold with
    // the literal on either side.
    if (op == "*") return litVal == 1 || litVal == -1 || log2OfPowerOfTwo(litVal).has_value();
    if (op == "+" || op == "|" || op == "^") return litVal == 0;
    if (literalWasLhs) {
        // "-"/"/"/"<<"/">>" aren't commutative, and there's no
        // identity for the literal on their left anyway (`0 - x` is
        // `-x`, not `x`; `1 / x` and `k << x` aren't `x` for any k),
        // so only `x <op> literal` applies past this point.
        return false;
    }
    if (op == "/") return litVal == 1 || litVal == -1;
    if (op == "-" || op == "<<" || op == ">>") return litVal == 0;
    return false;
}

// Rewrites `nonLit <op> litVal` (or `litVal <op> nonLit`, per
// `literalWasLhs`) into its cheaper equivalent. Only called once
// hasIdentityRewrite has already confirmed one applies, so every
// branch here is a direct match.
ir::ExprPtr applyIdentityRewrite(const std::string &op, ir::ExprPtr nonLit, long litVal, int line) {
    if (op == "*") {
        if (litVal == 1) return nonLit;
        if (litVal == -1) return makeNegate(std::move(nonLit), line);
        return makeShift("<<", std::move(nonLit), *log2OfPowerOfTwo(litVal), line);
    }
    if (op == "/" && litVal == -1) return makeNegate(std::move(nonLit), line);
    return nonLit;  // "/" by 1, "+"/"|"/"^"/"-" with 0, "<<"/">>" by 0
}

// Folds `e`'s own children first, then tries to rewrite `e` itself in
// place - same bottom-up shape as ConstantFolding::foldExpr.
void simplifyExpr(ir::ExprPtr &e, bool &changed) {
    if (!e) return;
    simplifyExpr(e->lhs, changed);
    simplifyExpr(e->rhs, changed);
    for (auto &arg : e->args) simplifyExpr(arg, changed);

    if (e->kind != ir::ExprKind::Binary) return;
    bool lhsLit = e->lhs->kind == ir::ExprKind::IntLit;
    bool rhsLit = e->rhs->kind == ir::ExprKind::IntLit;
    if (lhsLit == rhsLit) return;  // both literal (ConstantFolding's job) or neither

    long litVal = rhsLit ? e->rhs->intValue : e->lhs->intValue;
    if (!hasIdentityRewrite(e->op, litVal, /*literalWasLhs=*/lhsLit)) return;

    std::cerr << "algebraic-simplification: line " << e->line << ": rewrote `"
               << (lhsLit ? std::to_string(litVal) + " " + e->op + " <expr>"
                          : "<expr> " + e->op + " " + std::to_string(litVal))
               << "`\n";

    ir::ExprPtr nonLit = std::move(rhsLit ? e->lhs : e->rhs);
    e = applyIdentityRewrite(e->op, std::move(nonLit), litVal, e->line);
    changed = true;
}

void simplifyStmt(ir::Stmt *s, bool &changed) {
    if (!s) return;
    switch (s->kind) {
        case ir::StmtKind::Block:
            for (auto &child : s->body) simplifyStmt(child.get(), changed);
            return;
        case ir::StmtKind::If:
            simplifyExpr(s->cond, changed);
            simplifyStmt(s->thenStmt.get(), changed);
            simplifyStmt(s->elseStmt.get(), changed);
            return;
        case ir::StmtKind::While:
            simplifyExpr(s->cond, changed);
            simplifyStmt(s->loopBody.get(), changed);
            simplifyStmt(s->postStmt.get(), changed);
            return;
        case ir::StmtKind::Return:
        case ir::StmtKind::LocalDecl:
        case ir::StmtKind::ExprStmt:
            simplifyExpr(s->initOrReturnOrExpr, changed);
            return;
        case ir::StmtKind::Break:
        case ir::StmtKind::Continue:
        case ir::StmtKind::Empty:
        case ir::StmtKind::Asm:
            return;
    }
}

}  // namespace

bool AlgebraicSimplification::run(ir::Program &prog) const {
    bool changed = false;
    for (auto &fb : prog.functionBodies) simplifyStmt(fb.body.get(), changed);
    return changed;
}

}  // namespace pcc
