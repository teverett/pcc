#include "BuildAst.h"

#include <stdexcept>

#include "ANTLRInputStream.h"
#include "BaseErrorListener.h"
#include "CommonTokenStream.h"
#include "PccAsmLexer.h"
#include "PccAsmParser.h"

namespace pcc {

namespace {

[[noreturn]] void err(antlr4::ParserRuleContext *ctx, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(ctx->getStart()->getLine()) + ": " + msg);
}

// `asm { ... }`'s body is never parsed as part of normal compilation
// - it's copied verbatim into whatever text the backend builds (see
// grammar/CLexer.g4's own comment on `asm`) - but that means a typo
// inside one would otherwise go completely unnoticed until/unless
// something later actually assembles the output (`--format=rim`/
// `bin`/...), surfacing as a bare, uncaught exception from deep
// inside Pdp7Assembler.cpp/I8086Assembler.cpp with no connection back
// to the C source at all. Checking it here - against the exact same
// grammar (grammar/PccAsm.g4) and parser those assemblers themselves
// use, parse-only, no codegen - catches that right away, anchored to
// the real `asm { ... }`'s own line, the same way every other syntax
// error in this compiler is reported.
class AsmSyntaxErrorListener : public antlr4::BaseErrorListener {
   public:
    void syntaxError(antlr4::Recognizer *, antlr4::Token *, size_t line, size_t col, const std::string &msg,
                      std::exception_ptr) override {
        if (firstError.empty()) {
            firstError = std::to_string(line) + ":" + std::to_string(col) + ": " + msg;
        }
    }
    std::string firstError;
};

void checkAsmSyntax(const std::string &asmText, int sourceLine) {
    std::string text = asmText;
    if (!text.empty() && text.back() != '\n') text += "\n";

    antlr4::ANTLRInputStream input(text);
    PccAsmLexer lexer(&input);
    AsmSyntaxErrorListener errors;
    lexer.removeErrorListeners();
    lexer.addErrorListener(&errors);

    antlr4::CommonTokenStream tokens(&lexer);
    PccAsmParser parser(&tokens);
    parser.removeErrorListeners();
    parser.addErrorListener(&errors);

    parser.program();

    if (!errors.firstError.empty()) {
        throw std::runtime_error("line " + std::to_string(sourceLine) + ": asm { ... } at " + errors.firstError);
    }
}

ExprPtr buildExpr(CParser::ExprContext *ctx);

std::vector<long> buildArrayInit(CParser::ArrayInitContext *ctx) {
    std::vector<long> values;
    if (!ctx) return values;
    for (auto *tok : ctx->NUMBER()) values.push_back(std::stol(tok->getText()));
    return values;
}

void padArrayInit(std::vector<long> &values, int size, antlr4::ParserRuleContext *ctx) {
    if (static_cast<int>(values.size()) > size) {
        err(ctx, "array initializer has more elements (" + std::to_string(values.size()) +
                     ") than the array's declared size (" + std::to_string(size) + ")");
    }
    values.resize(size, 0);
}

ExprPtr buildCondExpr(CParser::CondExprContext *ctx);
StmtPtr buildStmt(CParser::StatementContext *ctx);
StmtPtr buildSimpleStmt(CParser::SimpleStmtContext *ctx);

ExprPtr makeBinary(antlr4::ParserRuleContext *ctx, const std::string &op, ExprPtr lhs, ExprPtr rhs) {
    auto e = std::make_unique<Expr>();
    e->kind = ExprKind::Binary;
    e->line = ctx->getStart()->getLine();
    e->op = op;
    e->lhs = std::move(lhs);
    e->rhs = std::move(rhs);
    return e;
}

ExprPtr buildCondExpr(CParser::CondExprContext *ctx) {
    if (auto *c = dynamic_cast<CParser::CallExprContext *>(ctx)) {
        auto e = std::make_unique<Expr>();
        e->kind = ExprKind::Call;
        e->line = c->getStart()->getLine();
        e->name = c->IDENT()->getText();
        if (auto *al = c->argList()) {
            for (auto *ae : al->expr()) e->args.push_back(buildExpr(ae));
        }
        return e;
    }
    if (auto *c = dynamic_cast<CParser::IndexExprContext *>(ctx)) {
        auto e = std::make_unique<Expr>();
        e->kind = ExprKind::Index;
        e->line = c->getStart()->getLine();
        e->name = c->IDENT()->getText();
        e->lhs = buildExpr(c->expr());
        return e;
    }
    if (auto *c = dynamic_cast<CParser::VarExprContext *>(ctx)) {
        auto e = std::make_unique<Expr>();
        e->kind = ExprKind::Var;
        e->line = c->getStart()->getLine();
        e->name = c->IDENT()->getText();
        return e;
    }
    if (auto *c = dynamic_cast<CParser::NumExprContext *>(ctx)) {
        auto e = std::make_unique<Expr>();
        e->kind = ExprKind::IntLit;
        e->line = c->getStart()->getLine();
        e->intValue = std::stol(c->NUMBER()->getText());
        return e;
    }
    if (auto *c = dynamic_cast<CParser::CharExprContext *>(ctx)) {
        auto e = std::make_unique<Expr>();
        e->kind = ExprKind::IntLit;
        e->line = c->getStart()->getLine();
        std::string text = c->CHARLIT()->getText();  // "'x'"
        e->intValue = static_cast<unsigned char>(text[1]);
        return e;
    }
    if (auto *c = dynamic_cast<CParser::ParenExprContext *>(ctx)) {
        return buildExpr(c->expr());
    }
    if (auto *c = dynamic_cast<CParser::UnaryExprContext *>(ctx)) {
        auto e = std::make_unique<Expr>();
        e->kind = ExprKind::Unary;
        e->line = c->getStart()->getLine();
        e->op = c->op->getText();
        e->rhs = buildCondExpr(c->condExpr());
        return e;
    }
    if (auto *c = dynamic_cast<CParser::DerefExprContext *>(ctx)) {
        auto e = std::make_unique<Expr>();
        e->kind = ExprKind::Unary;
        e->line = c->getStart()->getLine();
        e->op = "*";
        e->rhs = buildCondExpr(c->condExpr());
        return e;
    }
    if (auto *c = dynamic_cast<CParser::AddrOfExprContext *>(ctx)) {
        auto e = std::make_unique<Expr>();
        e->kind = ExprKind::Unary;
        e->line = c->getStart()->getLine();
        e->op = "&";
        e->rhs = buildCondExpr(c->condExpr());
        return e;
    }
    if (auto *c = dynamic_cast<CParser::MulExprContext *>(ctx)) {
        return makeBinary(c, c->op->getText(), buildCondExpr(c->condExpr(0)), buildCondExpr(c->condExpr(1)));
    }
    if (auto *c = dynamic_cast<CParser::AddExprContext *>(ctx)) {
        return makeBinary(c, c->op->getText(), buildCondExpr(c->condExpr(0)), buildCondExpr(c->condExpr(1)));
    }
    if (auto *c = dynamic_cast<CParser::ShiftExprContext *>(ctx)) {
        return makeBinary(c, c->op->getText(), buildCondExpr(c->condExpr(0)), buildCondExpr(c->condExpr(1)));
    }
    if (auto *c = dynamic_cast<CParser::RelExprContext *>(ctx)) {
        return makeBinary(c, c->op->getText(), buildCondExpr(c->condExpr(0)), buildCondExpr(c->condExpr(1)));
    }
    if (auto *c = dynamic_cast<CParser::EqExprContext *>(ctx)) {
        return makeBinary(c, c->op->getText(), buildCondExpr(c->condExpr(0)), buildCondExpr(c->condExpr(1)));
    }
    if (auto *c = dynamic_cast<CParser::BitAndExprContext *>(ctx)) {
        return makeBinary(c, "&", buildCondExpr(c->condExpr(0)), buildCondExpr(c->condExpr(1)));
    }
    if (auto *c = dynamic_cast<CParser::BitXorExprContext *>(ctx)) {
        return makeBinary(c, "^", buildCondExpr(c->condExpr(0)), buildCondExpr(c->condExpr(1)));
    }
    if (auto *c = dynamic_cast<CParser::BitOrExprContext *>(ctx)) {
        return makeBinary(c, "|", buildCondExpr(c->condExpr(0)), buildCondExpr(c->condExpr(1)));
    }
    if (auto *c = dynamic_cast<CParser::AndExprContext *>(ctx)) {
        return makeBinary(c, "&&", buildCondExpr(c->condExpr(0)), buildCondExpr(c->condExpr(1)));
    }
    if (auto *c = dynamic_cast<CParser::OrExprContext *>(ctx)) {
        return makeBinary(c, "||", buildCondExpr(c->condExpr(0)), buildCondExpr(c->condExpr(1)));
    }
    err(ctx, "internal: unhandled condExpr alternative");
}

ExprPtr buildExpr(CParser::ExprContext *ctx) {
    if (auto *c = dynamic_cast<CParser::AssignDerefExprContext *>(ctx)) {
        auto e = std::make_unique<Expr>();
        e->kind = ExprKind::DerefAssign;
        e->line = c->getStart()->getLine();
        e->lhs = buildCondExpr(c->ptr);
        e->rhs = buildExpr(c->val);
        return e;
    }
    if (auto *c = dynamic_cast<CParser::AssignIndexExprContext *>(ctx)) {
        auto e = std::make_unique<Expr>();
        e->kind = ExprKind::Assign;
        e->line = c->getStart()->getLine();
        e->name = c->IDENT()->getText();
        e->lhs = buildExpr(c->idx);
        e->rhs = buildExpr(c->val);
        return e;
    }
    if (auto *c = dynamic_cast<CParser::AssignExprContext *>(ctx)) {
        auto e = std::make_unique<Expr>();
        e->kind = ExprKind::Assign;
        e->line = c->getStart()->getLine();
        e->name = c->IDENT()->getText();
        e->rhs = buildExpr(c->expr());
        return e;
    }
    if (auto *c = dynamic_cast<CParser::ExprCondContext *>(ctx)) {
        return buildCondExpr(c->condExpr());
    }
    err(ctx, "internal: unhandled expr alternative");
}

StmtPtr buildSimpleStmt(CParser::SimpleStmtContext *ctx) {
    if (auto *c = dynamic_cast<CParser::LocalArrayDeclContext *>(ctx)) {
        auto s = std::make_unique<Stmt>();
        s->kind = StmtKind::LocalDecl;
        s->line = c->getStart()->getLine();
        s->declName = c->IDENT()->getText();
        s->arraySize = std::stoi(c->size->getText());
        if (s->arraySize <= 0) err(c, "array size must be a positive integer");
        s->arrayInit = buildArrayInit(c->arrayInit());
        padArrayInit(s->arrayInit, s->arraySize, c);
        return s;
    }
    if (auto *c = dynamic_cast<CParser::LocalDeclContext *>(ctx)) {
        auto s = std::make_unique<Stmt>();
        s->kind = StmtKind::LocalDecl;
        s->line = c->getStart()->getLine();
        s->declName = c->IDENT()->getText();
        if (auto *e = c->expr()) s->initOrReturnOrExpr = buildExpr(e);
        return s;
    }
    if (auto *c = dynamic_cast<CParser::ExprStmtContext *>(ctx)) {
        auto s = std::make_unique<Stmt>();
        s->kind = StmtKind::ExprStmt;
        s->line = c->getStart()->getLine();
        s->initOrReturnOrExpr = buildExpr(c->expr());
        return s;
    }
    err(ctx, "internal: unhandled simpleStmt alternative");
}

StmtPtr buildBlock(CParser::BlockContext *ctx) {
    auto s = std::make_unique<Stmt>();
    s->kind = StmtKind::Block;
    s->line = ctx->getStart()->getLine();
    for (auto *st : ctx->statement()) s->body.push_back(buildStmt(st));
    return s;
}

StmtPtr buildStmt(CParser::StatementContext *ctx) {
    if (auto *c = dynamic_cast<CParser::BlockStmtContext *>(ctx)) return buildBlock(c->block());

    if (auto *c = dynamic_cast<CParser::IfStmtContext *>(ctx)) {
        auto s = std::make_unique<Stmt>();
        s->kind = StmtKind::If;
        s->line = c->getStart()->getLine();
        s->cond = buildExpr(c->expr());
        s->thenStmt = buildStmt(c->thenS);
        if (c->elseS) s->elseStmt = buildStmt(c->elseS);
        return s;
    }

    if (auto *c = dynamic_cast<CParser::WhileStmtContext *>(ctx)) {
        auto s = std::make_unique<Stmt>();
        s->kind = StmtKind::While;
        s->line = c->getStart()->getLine();
        s->cond = buildExpr(c->expr());
        s->loopBody = buildStmt(c->statement());
        return s;
    }

    if (auto *c = dynamic_cast<CParser::ForStmtContext *>(ctx)) {
        int line = c->getStart()->getLine();

        // Desugar: { init; while (cond) { body; post; } }
        StmtPtr init = c->init ? buildSimpleStmt(c->init) : nullptr;
        ExprPtr cond = c->cond ? buildExpr(c->cond) : nullptr;
        StmtPtr post = c->post ? buildSimpleStmt(c->post) : nullptr;
        StmtPtr body = buildStmt(c->statement());

        auto whileBody = std::make_unique<Stmt>();
        whileBody->kind = StmtKind::Block;
        whileBody->line = line;
        whileBody->body.push_back(std::move(body));
        if (post) whileBody->body.push_back(std::move(post));

        auto whileStmt = std::make_unique<Stmt>();
        whileStmt->kind = StmtKind::While;
        whileStmt->line = line;
        if (cond) {
            whileStmt->cond = std::move(cond);
        } else {
            whileStmt->cond = std::make_unique<Expr>();
            whileStmt->cond->kind = ExprKind::IntLit;
            whileStmt->cond->intValue = 1;
        }
        whileStmt->loopBody = std::move(whileBody);

        auto outer = std::make_unique<Stmt>();
        outer->kind = StmtKind::Block;
        outer->line = line;
        if (init) outer->body.push_back(std::move(init));
        outer->body.push_back(std::move(whileStmt));
        return outer;
    }

    if (auto *c = dynamic_cast<CParser::ReturnStmtContext *>(ctx)) {
        auto s = std::make_unique<Stmt>();
        s->kind = StmtKind::Return;
        s->line = c->getStart()->getLine();
        if (auto *e = c->expr()) s->initOrReturnOrExpr = buildExpr(e);
        return s;
    }

    if (auto *c = dynamic_cast<CParser::AsmStmtContext *>(ctx)) {
        auto s = std::make_unique<Stmt>();
        s->kind = StmtKind::Asm;
        s->line = c->getStart()->getLine();
        if (c->text) s->asmText = c->text->getText();
        checkAsmSyntax(s->asmText, s->line);
        return s;
    }

    if (auto *c = dynamic_cast<CParser::SimpleStmtWrapperContext *>(ctx)) {
        return buildSimpleStmt(c->simpleStmt());
    }

    if (dynamic_cast<CParser::EmptyStmtContext *>(ctx)) {
        auto s = std::make_unique<Stmt>();
        s->kind = StmtKind::Empty;
        return s;
    }

    err(ctx, "internal: unhandled statement alternative");
}

}  // namespace

Program buildProgram(CParser::ProgramContext *ctx) {
    Program prog;
    for (auto *top : ctx->topLevelDecl()) {
        if (auto *fd = top->functionDecl()) {
            if (!fd->block()) continue;  // prototype only, nothing to generate

            FunctionDecl fn;
            fn.name = fd->IDENT()->getText();
            fn.line = fd->getStart()->getLine();
            if (auto *pl = fd->paramList()) {
                for (auto *p : pl->param()) fn.params.push_back(Param{p->IDENT()->getText()});
            }
            if (fd->addr) {
                fn.hasAddress = true;
                fn.address = std::stol(fd->addr->getText());
            }
            fn.body = buildBlock(fd->block());
            prog.functions.push_back(std::move(fn));
        } else if (auto *gd = top->globalDecl()) {
            GlobalVar g;
            g.line = gd->getStart()->getLine();
            if (auto *arr = dynamic_cast<CParser::GlobalArrayDeclContext *>(gd)) {
                g.name = arr->IDENT()->getText();
                g.arraySize = std::stoi(arr->size->getText());
                if (g.arraySize <= 0) err(arr, "array size must be a positive integer");
                g.arrayInit = buildArrayInit(arr->arrayInit());
                padArrayInit(g.arrayInit, g.arraySize, arr);
                if (arr->addr) {
                    g.hasAddress = true;
                    g.address = std::stol(arr->addr->getText());
                }
            } else if (auto *sc = dynamic_cast<CParser::GlobalScalarDeclContext *>(gd)) {
                g.name = sc->IDENT()->getText();
                if (auto *num = sc->val) {
                    g.hasInit = true;
                    g.initValue = std::stol(num->getText());
                    if (sc->sign) g.initValue = -g.initValue;
                }
                if (sc->addr) {
                    g.hasAddress = true;
                    g.address = std::stol(sc->addr->getText());
                }
            }
            prog.globals.push_back(std::move(g));
        }
    }
    return prog;
}

}  // namespace pcc
