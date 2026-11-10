#include "optimizers/constant_folding/ConstantFolding.h"

#include <iostream>
#include <optional>
#include <string>

namespace rcc {

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
    return std::nullopt;  // "*"/"&" (deref/address-of): never foldable
}

// nullopt means "don't fold": div/mod by zero or a shift outside [0, 63]
// would need host-undefined-behavior to evaluate here.
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

// Folds children first (bottom-up), then tries to fold `e` itself.
void foldExpr(ir::ExprPtr &e, bool &changed, const std::string &sourceFile) {
    if (!e) return;
    foldExpr(e->lhs, changed, sourceFile);
    foldExpr(e->rhs, changed, sourceFile);
    for (auto &arg : e->args) foldExpr(arg, changed, sourceFile);

    if (e->kind == ir::ExprKind::Unary && e->rhs->kind == ir::ExprKind::IntLit) {
        if (std::optional<long> v = evalUnary(e->op, e->rhs->intValue)) {
            // Unlike the Binary case below, the printed value often doesn't
            // change (e.g. `-64` is parsed as Unary("-", IntLit(64)), whose
            // fold prints as "-64" both before and after) - what's actually
            // collapsing here is two IR nodes into one, not the value, so
            // say that explicitly instead of a misleading "folded X to X".
            std::cerr << sourceFile << ": constant-folding: line " << e->line << ": collapsed unary `" << e->op
                       << e->rhs->intValue << "` into the single literal `" << *v << "`\n";
            e = makeIntLit(*v, e->line, e->type);
            changed = true;
        }
    } else if (e->kind == ir::ExprKind::Binary && e->lhs->kind == ir::ExprKind::IntLit &&
               e->rhs->kind == ir::ExprKind::IntLit) {
        if (std::optional<long> v = evalBinary(e->op, e->lhs->intValue, e->rhs->intValue)) {
            std::cerr << sourceFile << ": constant-folding: line " << e->line << ": folded `" << e->lhs->intValue
                       << " " << e->op << " " << e->rhs->intValue << "` to `" << *v << "`\n";
            e = makeIntLit(*v, e->line, e->type);
            changed = true;
        }
    }
}

void foldStmt(ir::Stmt *s, bool &changed, const std::string &sourceFile) {
    if (!s) return;
    switch (s->kind) {
        case ir::StmtKind::Block:
            for (auto &child : s->body) foldStmt(child.get(), changed, sourceFile);
            return;
        case ir::StmtKind::If:
            foldExpr(s->cond, changed, sourceFile);
            foldStmt(s->thenStmt.get(), changed, sourceFile);
            foldStmt(s->elseStmt.get(), changed, sourceFile);
            return;
        case ir::StmtKind::While:
            foldExpr(s->cond, changed, sourceFile);
            foldStmt(s->loopBody.get(), changed, sourceFile);
            foldStmt(s->postStmt.get(), changed, sourceFile);
            return;
        case ir::StmtKind::Return:
        case ir::StmtKind::LocalDecl:
        case ir::StmtKind::ExprStmt:
            foldExpr(s->initOrReturnOrExpr, changed, sourceFile);
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

bool ConstantFolding::run(ir::Program &prog, const std::string &sourceFile) const {
    bool changed = false;
    for (auto &fb : prog.functionBodies) foldStmt(fb.body.get(), changed, sourceFile);
    return changed;
}

}  // namespace rcc
