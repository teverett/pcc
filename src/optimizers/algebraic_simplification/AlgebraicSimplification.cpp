#include "optimizers/algebraic_simplification/AlgebraicSimplification.h"

#include <iostream>
#include <optional>
#include <string>

namespace rcc {

namespace {

ir::ExprPtr makeIntLit(long value, int line) {
    auto lit = std::make_unique<ir::Expr>();
    lit->kind = ir::ExprKind::IntLit;
    lit->line = line;
    lit->intValue = value;
    return lit;
}

ir::ExprPtr makeNegate(ir::ExprPtr operand, int line, Type type) {
    auto neg = std::make_unique<ir::Expr>();
    neg->kind = ir::ExprKind::Unary;
    neg->op = "-";
    neg->line = line;
    neg->type = type;
    neg->rhs = std::move(operand);
    return neg;
}

ir::ExprPtr makeShift(const std::string &op, ir::ExprPtr operand, long amount, int line, Type type) {
    auto shift = std::make_unique<ir::Expr>();
    shift->kind = ir::ExprKind::Binary;
    shift->op = op;
    shift->line = line;
    shift->type = type;
    shift->lhs = std::move(operand);
    shift->rhs = makeIntLit(amount, line);
    return shift;
}

// n's log2, or nullopt if n isn't a positive power of two.
std::optional<long> log2OfPowerOfTwo(long n) {
    if (n <= 0 || (n & (n - 1)) != 0) return std::nullopt;
    long k = 0;
    while (n > 1) {
        n >>= 1;
        ++k;
    }
    return k;
}

// `op` is e's operator with exactly one of lhs/rhs an IntLit of value
// `litVal`; `literalWasLhs` says which side.
bool hasIdentityRewrite(const std::string &op, long litVal, bool literalWasLhs) {
    // "*"/"+"/"|"/"^" are commutative: identity holds on either side.
    if (op == "*") return litVal == 1 || litVal == -1 || log2OfPowerOfTwo(litVal).has_value();
    if (op == "+" || op == "|" || op == "^") return litVal == 0;
    if (literalWasLhs) {
        // "-"/"/"/"<<"/">>" aren't commutative and have no identity with
        // the literal on the left.
        return false;
    }
    if (op == "/") return litVal == 1 || litVal == -1;
    if (op == "-" || op == "<<" || op == ">>") return litVal == 0;
    return false;
}

// Only called once hasIdentityRewrite confirms a rewrite applies.
ir::ExprPtr applyIdentityRewrite(const std::string &op, ir::ExprPtr nonLit, long litVal, int line, Type type) {
    if (op == "*") {
        if (litVal == 1) return nonLit;
        if (litVal == -1) return makeNegate(std::move(nonLit), line, type);
        return makeShift("<<", std::move(nonLit), *log2OfPowerOfTwo(litVal), line, type);
    }
    if (op == "/" && litVal == -1) return makeNegate(std::move(nonLit), line, type);
    return nonLit;  // "/" by 1, "+"/"|"/"^"/"-" with 0, "<<"/">>" by 0
}

// Simplifies children first, then tries to rewrite `e` itself in place.
void simplifyExpr(ir::ExprPtr &e, bool &changed, const std::string &sourceFile) {
    if (!e) return;
    simplifyExpr(e->lhs, changed, sourceFile);
    simplifyExpr(e->rhs, changed, sourceFile);
    for (auto &arg : e->args) simplifyExpr(arg, changed, sourceFile);

    if (e->kind != ir::ExprKind::Binary) return;
    bool lhsLit = e->lhs->kind == ir::ExprKind::IntLit;
    bool rhsLit = e->rhs->kind == ir::ExprKind::IntLit;
    if (lhsLit == rhsLit) return;  // both literal (ConstantFolding's job) or neither

    long litVal = rhsLit ? e->rhs->intValue : e->lhs->intValue;
    if (!hasIdentityRewrite(e->op, litVal, /*literalWasLhs=*/lhsLit)) return;

    std::cerr << sourceFile << ": algebraic-simplification: line " << e->line << ": rewrote `"
               << (lhsLit ? std::to_string(litVal) + " " + e->op + " <expr>"
                          : "<expr> " + e->op + " " + std::to_string(litVal))
               << "`\n";

    ir::ExprPtr nonLit = std::move(rhsLit ? e->lhs : e->rhs);
    Type resultType = e->type;
    e = applyIdentityRewrite(e->op, std::move(nonLit), litVal, e->line, resultType);
    changed = true;
}

void simplifyStmt(ir::Stmt *s, bool &changed, const std::string &sourceFile) {
    if (!s) return;
    switch (s->kind) {
        case ir::StmtKind::Block:
            for (auto &child : s->body) simplifyStmt(child.get(), changed, sourceFile);
            return;
        case ir::StmtKind::If:
            simplifyExpr(s->cond, changed, sourceFile);
            simplifyStmt(s->thenStmt.get(), changed, sourceFile);
            simplifyStmt(s->elseStmt.get(), changed, sourceFile);
            return;
        case ir::StmtKind::While:
            simplifyExpr(s->cond, changed, sourceFile);
            simplifyStmt(s->loopBody.get(), changed, sourceFile);
            simplifyStmt(s->postStmt.get(), changed, sourceFile);
            return;
        case ir::StmtKind::Return:
        case ir::StmtKind::LocalDecl:
        case ir::StmtKind::ExprStmt:
            simplifyExpr(s->initOrReturnOrExpr, changed, sourceFile);
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

}  // namespace

bool AlgebraicSimplification::run(ir::Program &prog, const std::string &sourceFile) const {
    bool changed = false;
    for (auto &fb : prog.functionBodies) simplifyStmt(fb.body.get(), changed, sourceFile);
    return changed;
}

}  // namespace rcc
