#include "BuildAst.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>

#include "ANTLRInputStream.h"
#include "BaseErrorListener.h"
#include "CommonTokenStream.h"
#include "RccAsmLexer.h"
#include "RccAsmParser.h"

namespace rcc {

namespace {

[[noreturn]] void err(antlr4::ParserRuleContext *ctx, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(ctx->getStart()->getLine()) + ": " + msg);
}

// `asm { ... }` text is copied verbatim and only ever parsed by the backend
// assembler at codegen time; this runs the same grammar/parser eagerly here
// so a typo is reported at the asm block's own line instead of surfacing
// later as an uncaught exception deep in a backend assembler.
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
    RccAsmLexer lexer(&input);
    AsmSyntaxErrorListener errors;
    lexer.removeErrorListeners();
    lexer.addErrorListener(&errors);

    antlr4::CommonTokenStream tokens(&lexer);
    RccAsmParser parser(&tokens);
    parser.removeErrorListeners();
    parser.addErrorListener(&errors);

    parser.program();

    if (!errors.firstError.empty()) {
        throw std::runtime_error("line " + std::to_string(sourceLine) + ": asm { ... } at " + errors.firstError);
    }
}

ExprPtr buildExpr(CParser::ExprContext *ctx);

// A decimal digit string's value as the raw 64-bit pattern `Expr::intValue`/callers store it
// in - parsed unsigned so a literal up to 2^64-1 (e.g. an `ULL` one past `LONG_MAX`) wraps the
// same way C itself defines it, instead of `std::stol`'s signed range rejecting it outright.
long parseDigits(const std::string &text, antlr4::ParserRuleContext *ctx, const char *what, int base = 10) {
    unsigned long long v = 0;
    try {
        v = std::stoull(text, nullptr, base);
    } catch (const std::out_of_range &) {
        err(ctx, std::string(what) + " '" + text + "' is too large to represent in 64 bits");
    }
    return static_cast<long>(v);
}

// `size=`/`addr=`/array-initializer NUMBERs are untyped compiler-internal
// integers and reject an integer suffix, unlike a general NUMBER token.
long parsePlainNumber(antlr4::Token *tok, antlr4::ParserRuleContext *ctx, const char *what) {
    std::string text = tok->getText();
    for (char ch : text) {
        if (ch < '0' || ch > '9') err(ctx, std::string(what) + " may not have a 'u'/'l' integer suffix");
    }
    return parseDigits(text, ctx, what);
}

// Parses a NUMBER token's integer suffix (ISO C spelling: u/U once, l/L
// zero to two times, not mixed case) into the literal's Type.
Type parseNumberSuffix(const std::string &suffix, antlr4::ParserRuleContext *ctx) {
    bool isUnsigned = false;
    int longCount = 0;
    char longChar = 0;
    for (char ch : suffix) {
        if (ch == 'u' || ch == 'U') {
            if (isUnsigned) err(ctx, "invalid integer suffix '" + suffix + "' - 'u'/'U' given twice");
            isUnsigned = true;
        } else if (ch == 'l' || ch == 'L') {
            if (longCount == 2) err(ctx, "invalid integer suffix '" + suffix + "' - at most 'll'/'LL' is allowed");
            if (longCount == 1 && ch != longChar) {
                err(ctx, "invalid integer suffix '" + suffix + "' - 'll'/'LL' must not mix case");
            }
            longChar = ch;
            longCount++;
        } else {
            err(ctx, "invalid integer suffix '" + suffix + "'");
        }
    }
    BaseType base = longCount == 2 ? BaseType::LongLong : longCount == 1 ? BaseType::Long : BaseType::Int;
    return Type{base, isUnsigned};
}

// Splits a NUMBER token's text into its digits and its suffix, and
// returns both the parsed value and the suffix's Type.
void parseIntLiteral(const std::string &text, antlr4::ParserRuleContext *ctx, long &value, Type &type) {
    size_t digitsEnd = 0;
    bool isHex = text.size() >= 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X');
    if (isHex) {
        digitsEnd = 2;
        while (digitsEnd < text.size() && std::isxdigit(static_cast<unsigned char>(text[digitsEnd]))) digitsEnd++;
    } else {
        while (digitsEnd < text.size() && text[digitsEnd] >= '0' && text[digitsEnd] <= '9') digitsEnd++;
    }
    // Base 0 makes stoull auto-detect the same 0x/0/decimal prefixing the lexer's NUMBER rule admits.
    value = parseDigits(text.substr(0, digitsEnd), ctx, "an integer literal", 0);
    type = parseNumberSuffix(text.substr(digitsEnd), ctx);
}

// Strips the surrounding quotes from a STRINGLIT token and resolves its
// escapes; the grammar admits only \\ \" \n \t \r \0, so no other case
// can occur here.
std::string decodeStringLit(const std::string &raw) {
    std::string out;
    out.reserve(raw.size());
    for (size_t i = 1; i + 1 < raw.size(); i++) {
        char c = raw[i];
        if (c != '\\') {
            out.push_back(c);
            continue;
        }
        i++;
        switch (raw[i]) {
            case 'n':
                out.push_back('\n');
                break;
            case 't':
                out.push_back('\t');
                break;
            case 'r':
                out.push_back('\r');
                break;
            case '0':
                out.push_back('\0');
                break;
            default:
                out.push_back(raw[i]);  // \\ or \" - the backslash itself is dropped
                break;
        }
    }
    return out;
}

Type buildBaseType(CParser::BaseTypeSpecContext *ctx) {
    bool isUnsigned = false;
    if (auto *c = dynamic_cast<CParser::CharTypeContext *>(ctx)) {
        isUnsigned = c->UNSIGNED_KW() != nullptr;
        return Type{BaseType::Char, isUnsigned};
    }
    if (auto *c = dynamic_cast<CParser::ShortTypeContext *>(ctx)) {
        isUnsigned = c->UNSIGNED_KW() != nullptr;
        return Type{BaseType::Short, isUnsigned};
    }
    if (auto *c = dynamic_cast<CParser::IntTypeContext *>(ctx)) {
        isUnsigned = c->UNSIGNED_KW() != nullptr;
        return Type{BaseType::Int, isUnsigned};
    }
    if (auto *c = dynamic_cast<CParser::LongLongTypeContext *>(ctx)) {
        isUnsigned = c->UNSIGNED_KW() != nullptr;
        return Type{BaseType::LongLong, isUnsigned};
    }
    if (auto *c = dynamic_cast<CParser::LongTypeContext *>(ctx)) {
        isUnsigned = c->UNSIGNED_KW() != nullptr;
        return Type{BaseType::Long, isUnsigned};
    }
    if (auto *c = dynamic_cast<CParser::SignedOrUnsignedTypeContext *>(ctx)) {
        isUnsigned = c->UNSIGNED_KW() != nullptr;
        return Type{BaseType::Int, isUnsigned};
    }
    if (auto *c = dynamic_cast<CParser::StructTypeContext *>(ctx)) {
        // Only the tag is recorded here - this layer has no TypeWidths, so
        // size/alignment resolution (and checking the tag is actually
        // defined) happens later, in IRBuilder's resolveStructType().
        Type t(BaseType::Struct);
        t.structTag = c->IDENT()->getText();
        return t;
    }
    err(ctx, "internal: unhandled baseTypeSpec alternative");
}

Type buildType(CParser::TypeSpecContext *ctx) {
    Type t = buildBaseType(ctx->baseTypeSpec());
    t.ptrDepth = static_cast<int>(ctx->STAR().size());
    return t;
}

// A function's return type: 'void' sets `isVoid` and leaves `type`
// default-constructed (unused by callers in that case).
void buildReturnType(CParser::ReturnTypeSpecContext *ctx, Type &type, bool &isVoid) {
    if (ctx->typeSpec()) {
        type = buildType(ctx->typeSpec());
        isVoid = false;
    } else {
        isVoid = true;
    }
}

std::vector<ArrayInitElem> buildArrayInit(CParser::ArrayInitContext *ctx) {
    std::vector<ArrayInitElem> values;
    if (!ctx) return values;
    for (auto *elem : ctx->elem) {
        ArrayInitElem e;
        if (auto *num = dynamic_cast<CParser::NumArrayElemContext *>(elem)) {
            e.value = parsePlainNumber(num->NUMBER()->getSymbol(), num, "an array initializer element");
            if (num->sign) e.value = -e.value;
        } else if (auto *sym = dynamic_cast<CParser::SymArrayElemContext *>(elem)) {
            e.isSymbol = true;
            e.symbolName = sym->IDENT()->getText();
        }
        values.push_back(std::move(e));
    }
    return values;
}

void padArrayInit(std::vector<ArrayInitElem> &values, int size, antlr4::ParserRuleContext *ctx) {
    if (static_cast<int>(values.size()) > size) {
        err(ctx, "array initializer has more elements (" + std::to_string(values.size()) +
                     ") than the array's declared size (" + std::to_string(size) + ")");
    }
    values.resize(size);  // default ArrayInitElem is a literal 0
}

ExprPtr buildCondExpr(CParser::CondExprContext *ctx);
ExprPtr buildExpr(CParser::ExprContext *ctx);
StmtPtr buildStmt(CParser::StatementContext *ctx);
StmtPtr buildSimpleStmt(CParser::SimpleStmtContext *ctx);
ExprPtr makeIncDec(antlr4::ParserRuleContext *ctx, const std::string &opToken, bool yieldOld);

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
    if (auto *c = dynamic_cast<CParser::PostIncDecVarExprContext *>(ctx)) {
        auto e = makeIncDec(c, c->op->getText(), /*yieldOld=*/true);
        e->kind = ExprKind::CompoundAssign;
        e->name = c->IDENT()->getText();
        return e;
    }
    if (auto *c = dynamic_cast<CParser::PostIncDecIndexExprContext *>(ctx)) {
        auto e = makeIncDec(c, c->op->getText(), /*yieldOld=*/true);
        e->kind = ExprKind::CompoundAssign;
        e->name = c->IDENT()->getText();
        e->lhs = buildExpr(c->idx);
        return e;
    }
    if (auto *c = dynamic_cast<CParser::PostIncDecDerefExprContext *>(ctx)) {
        auto e = makeIncDec(c, c->op->getText(), /*yieldOld=*/true);
        e->kind = ExprKind::CompoundDerefAssign;
        e->lhs = buildCondExpr(c->ptr);
        return e;
    }
    if (auto *c = dynamic_cast<CParser::PreIncDecVarExprContext *>(ctx)) {
        auto e = makeIncDec(c, c->op->getText(), /*yieldOld=*/false);
        e->kind = ExprKind::CompoundAssign;
        e->name = c->IDENT()->getText();
        return e;
    }
    if (auto *c = dynamic_cast<CParser::PreIncDecIndexExprContext *>(ctx)) {
        auto e = makeIncDec(c, c->op->getText(), /*yieldOld=*/false);
        e->kind = ExprKind::CompoundAssign;
        e->name = c->IDENT()->getText();
        e->lhs = buildExpr(c->idx);
        return e;
    }
    if (auto *c = dynamic_cast<CParser::PreIncDecDerefExprContext *>(ctx)) {
        auto e = makeIncDec(c, c->op->getText(), /*yieldOld=*/false);
        e->kind = ExprKind::CompoundDerefAssign;
        e->lhs = buildCondExpr(c->ptr);
        return e;
    }
    if (auto *c = dynamic_cast<CParser::TernaryExprContext *>(ctx)) {
        auto e = std::make_unique<Expr>();
        e->kind = ExprKind::Ternary;
        e->line = c->getStart()->getLine();
        e->lhs = buildCondExpr(c->cond);
        e->rhs = buildExpr(c->then);
        e->elseExpr = buildCondExpr(c->els);
        return e;
    }
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
    if (auto *c = dynamic_cast<CParser::CallPtrExprContext *>(ctx)) {
        auto e = std::make_unique<Expr>();
        e->kind = ExprKind::CallPtr;
        e->line = c->getStart()->getLine();
        e->lhs = buildExpr(c->expr());
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
        parseIntLiteral(c->NUMBER()->getText(), c, e->intValue, e->type);
        return e;
    }
    if (auto *c = dynamic_cast<CParser::CharExprContext *>(ctx)) {
        auto e = std::make_unique<Expr>();
        e->kind = ExprKind::IntLit;
        e->line = c->getStart()->getLine();
        std::string text = c->CHARLIT()->getText();  // "'x'"
        e->intValue = static_cast<unsigned char>(text[1]);  // type stays plain int, as in C
        return e;
    }
    if (auto *c = dynamic_cast<CParser::StringLitExprContext *>(ctx)) {
        auto e = std::make_unique<Expr>();
        e->kind = ExprKind::StringLit;
        e->line = c->getStart()->getLine();
        e->name = decodeStringLit(c->STRINGLIT()->getText());
        return e;
    }
    if (auto *c = dynamic_cast<CParser::ParenExprContext *>(ctx)) {
        return buildExpr(c->expr());
    }
    if (auto *c = dynamic_cast<CParser::CastExprContext *>(ctx)) {
        auto e = std::make_unique<Expr>();
        e->kind = ExprKind::Cast;
        e->line = c->getStart()->getLine();
        e->type = buildType(c->typeSpec());
        e->rhs = buildCondExpr(c->condExpr());
        return e;
    }
    if (auto *c = dynamic_cast<CParser::SizeofTypeExprContext *>(ctx)) {
        auto e = std::make_unique<Expr>();
        e->kind = ExprKind::Sizeof;
        e->line = c->getStart()->getLine();
        e->sizeofIsType = true;
        e->type = buildType(c->typeSpec());
        return e;
    }
    if (auto *c = dynamic_cast<CParser::SizeofExprExprContext *>(ctx)) {
        auto e = std::make_unique<Expr>();
        e->kind = ExprKind::Sizeof;
        e->line = c->getStart()->getLine();
        e->sizeofIsType = false;
        e->rhs = buildExpr(c->expr());
        return e;
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
    if (auto *c = dynamic_cast<CParser::MemberAccessExprContext *>(ctx)) {
        auto e = std::make_unique<Expr>();
        e->kind = ExprKind::MemberAccess;
        e->line = c->getStart()->getLine();
        e->viaPointer = false;
        e->name = c->name->getText();
        e->lhs = buildCondExpr(c->base);
        if (c->idx) e->memberIndex = buildExpr(c->idx);
        return e;
    }
    if (auto *c = dynamic_cast<CParser::MemberAccessPtrExprContext *>(ctx)) {
        auto e = std::make_unique<Expr>();
        e->kind = ExprKind::MemberAccess;
        e->line = c->getStart()->getLine();
        e->viaPointer = true;
        e->name = c->name->getText();
        e->lhs = buildCondExpr(c->base);
        if (c->idx) e->memberIndex = buildExpr(c->idx);
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

// Strips an assignOp token's trailing '=' ("+=" -> "+"), or returns ""
// for a bare '=' (plain, non-compound assignment).
std::string stripAssignEq(const std::string &opText) {
    if (opText == "=") return "";
    return opText.substr(0, opText.size() - 1);
}

ExprPtr buildExpr(CParser::ExprContext *ctx) {
    if (auto *c = dynamic_cast<CParser::AssignDerefExprContext *>(ctx)) {
        std::string op = stripAssignEq(c->op->getText());
        auto e = std::make_unique<Expr>();
        e->kind = op.empty() ? ExprKind::DerefAssign : ExprKind::CompoundDerefAssign;
        e->line = c->getStart()->getLine();
        e->op = op;
        e->lhs = buildCondExpr(c->ptr);
        e->rhs = buildExpr(c->val);
        return e;
    }
    if (auto *c = dynamic_cast<CParser::AssignIndexExprContext *>(ctx)) {
        std::string op = stripAssignEq(c->op->getText());
        auto e = std::make_unique<Expr>();
        e->kind = op.empty() ? ExprKind::Assign : ExprKind::CompoundAssign;
        e->line = c->getStart()->getLine();
        e->op = op;
        e->name = c->IDENT()->getText();
        e->lhs = buildExpr(c->idx);
        e->rhs = buildExpr(c->val);
        return e;
    }
    if (auto *c = dynamic_cast<CParser::AssignExprContext *>(ctx)) {
        std::string op = stripAssignEq(c->op->getText());
        auto e = std::make_unique<Expr>();
        e->kind = op.empty() ? ExprKind::Assign : ExprKind::CompoundAssign;
        e->line = c->getStart()->getLine();
        e->op = op;
        e->name = c->IDENT()->getText();
        e->rhs = buildExpr(c->val);
        return e;
    }
    if (auto *c = dynamic_cast<CParser::AssignMemberExprContext *>(ctx)) {
        std::string op = stripAssignEq(c->op->getText());
        auto e = std::make_unique<Expr>();
        e->kind = op.empty() ? ExprKind::MemberAssign : ExprKind::CompoundMemberAssign;
        e->line = c->getStart()->getLine();
        e->op = op;
        e->viaPointer = false;
        e->name = c->name->getText();
        e->lhs = buildCondExpr(c->base);
        if (c->idx) e->memberIndex = buildExpr(c->idx);
        e->rhs = buildExpr(c->val);
        return e;
    }
    if (auto *c = dynamic_cast<CParser::AssignMemberPtrExprContext *>(ctx)) {
        std::string op = stripAssignEq(c->op->getText());
        auto e = std::make_unique<Expr>();
        e->kind = op.empty() ? ExprKind::MemberAssign : ExprKind::CompoundMemberAssign;
        e->line = c->getStart()->getLine();
        e->op = op;
        e->viaPointer = true;
        e->name = c->name->getText();
        e->lhs = buildCondExpr(c->base);
        if (c->idx) e->memberIndex = buildExpr(c->idx);
        e->rhs = buildExpr(c->val);
        return e;
    }
    if (auto *c = dynamic_cast<CParser::ExprCondContext *>(ctx)) {
        return buildCondExpr(c->condExpr());
    }
    err(ctx, "internal: unhandled expr alternative");
}

// Builds the common CompoundAssign(DerefAssign) shape for '++'/'--';
// caller sets kind/name/lhs and whether yieldOld (postfix only).
ExprPtr makeIncDec(antlr4::ParserRuleContext *ctx, const std::string &opToken, bool yieldOld) {
    auto one = std::make_unique<Expr>();
    one->kind = ExprKind::IntLit;
    one->line = ctx->getStart()->getLine();
    one->intValue = 1;
    auto e = std::make_unique<Expr>();
    e->line = ctx->getStart()->getLine();
    e->op = (opToken == "++") ? "+" : "-";
    e->yieldOld = yieldOld;
    e->rhs = std::move(one);
    return e;
}

StmtPtr buildSimpleStmt(CParser::SimpleStmtContext *ctx) {
    if (auto *c = dynamic_cast<CParser::LocalArrayDeclContext *>(ctx)) {
        auto s = std::make_unique<Stmt>();
        s->kind = StmtKind::LocalDecl;
        s->line = c->getStart()->getLine();
        s->declName = c->IDENT()->getText();
        s->type = buildType(c->typeSpec());
        s->arraySize = static_cast<int>(parsePlainNumber(c->size, c, "an array size"));
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
        s->type = buildType(c->typeSpec());
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

        // Desugars to { init; while (cond) body; }; see AST.h's Stmt::postStmt
        // for why post is kept separate from the loop body.
        StmtPtr init = c->init ? buildSimpleStmt(c->init) : nullptr;
        ExprPtr cond = c->cond ? buildExpr(c->cond) : nullptr;
        StmtPtr post = c->post ? buildSimpleStmt(c->post) : nullptr;
        StmtPtr body = buildStmt(c->statement());

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
        whileStmt->loopBody = std::move(body);
        whileStmt->postStmt = std::move(post);

        auto outer = std::make_unique<Stmt>();
        outer->kind = StmtKind::Block;
        outer->line = line;
        if (init) outer->body.push_back(std::move(init));
        outer->body.push_back(std::move(whileStmt));
        return outer;
    }

    if (auto *c = dynamic_cast<CParser::SwitchStmtContext *>(ctx)) {
        auto s = std::make_unique<Stmt>();
        s->kind = StmtKind::Switch;
        s->line = c->getStart()->getLine();
        s->cond = buildExpr(c->expr());

        bool sawDefault = false;
        std::vector<long> seenValues;
        for (auto *sc : c->switchCase()) {
            SwitchCase sw;
            if (auto *caseLabel = dynamic_cast<CParser::CaseLabelContext *>(sc)) {
                sw.value = parsePlainNumber(caseLabel->val, caseLabel, "a case label");
                if (caseLabel->sign) sw.value = -sw.value;
                if (std::find(seenValues.begin(), seenValues.end(), sw.value) != seenValues.end()) {
                    err(caseLabel, "duplicate case value " + std::to_string(sw.value));
                }
                seenValues.push_back(sw.value);
                for (auto *st : caseLabel->statement()) sw.body.push_back(buildStmt(st));
            } else if (auto *defaultLabel = dynamic_cast<CParser::DefaultLabelContext *>(sc)) {
                if (sawDefault) err(defaultLabel, "switch statement has more than one 'default' label");
                sawDefault = true;
                sw.isDefault = true;
                for (auto *st : defaultLabel->statement()) sw.body.push_back(buildStmt(st));
            } else {
                err(sc, "internal: unhandled switchCase alternative");
            }
            s->cases.push_back(std::move(sw));
        }
        return s;
    }

    if (auto *c = dynamic_cast<CParser::ReturnStmtContext *>(ctx)) {
        auto s = std::make_unique<Stmt>();
        s->kind = StmtKind::Return;
        s->line = c->getStart()->getLine();
        if (auto *e = c->expr()) s->initOrReturnOrExpr = buildExpr(e);
        return s;
    }

    if (auto *c = dynamic_cast<CParser::BreakStmtContext *>(ctx)) {
        auto s = std::make_unique<Stmt>();
        s->kind = StmtKind::Break;
        s->line = c->getStart()->getLine();
        return s;
    }

    if (auto *c = dynamic_cast<CParser::ContinueStmtContext *>(ctx)) {
        auto s = std::make_unique<Stmt>();
        s->kind = StmtKind::Continue;
        s->line = c->getStart()->getLine();
        return s;
    }

    if (auto *c = dynamic_cast<CParser::GotoStmtContext *>(ctx)) {
        auto s = std::make_unique<Stmt>();
        s->kind = StmtKind::Goto;
        s->line = c->getStart()->getLine();
        s->declName = c->IDENT()->getText();
        return s;
    }

    if (auto *c = dynamic_cast<CParser::LabelStmtContext *>(ctx)) {
        auto s = std::make_unique<Stmt>();
        s->kind = StmtKind::Label;
        s->line = c->getStart()->getLine();
        s->declName = c->IDENT()->getText();
        s->thenStmt = buildStmt(c->statement());
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

StructDecl buildStructDecl(CParser::StructDeclContext *ctx) {
    StructDecl decl;
    decl.tag = ctx->IDENT(0)->getText();
    decl.line = ctx->getStart()->getLine();
    if (ctx->packedTag) {
        if (ctx->packedTag->getText() != "packed") {
            err(ctx, "unknown struct attribute '@" + ctx->packedTag->getText() +
                          "' - only '@packed' is supported");
        }
        decl.packed = true;
    }
    for (auto *m : ctx->structMember()) {
        StructMemberDecl member;
        member.name = m->IDENT()->getText();
        member.line = m->getStart()->getLine();
        member.type = buildType(m->typeSpec());
        if (m->size) {
            member.arraySize = static_cast<int>(parsePlainNumber(m->size, m, "a struct member's array size"));
            if (member.arraySize <= 0) err(m, "array size must be a positive integer");
        }
        decl.members.push_back(std::move(member));
    }
    return decl;
}

}  // namespace

Program buildProgram(CParser::ProgramContext *ctx) {
    Program prog;
    for (auto *top : ctx->topLevelDecl()) {
        if (auto *sd = top->structDecl()) {
            prog.structs.push_back(buildStructDecl(sd));
        } else if (auto *fd = top->functionDecl()) {
            if (auto *ext = dynamic_cast<CParser::ExternFunctionDeclContext *>(fd)) {
                FunctionDecl fn;
                fn.name = ext->IDENT()->getText();
                fn.line = ext->getStart()->getLine();
                buildReturnType(ext->returnTypeSpec(), fn.returnType, fn.isVoid);
                if (auto *pl = ext->paramList()) {
                    for (auto *p : pl->param()) {
                        fn.params.push_back(Param{p->IDENT()->getText(), buildType(p->typeSpec())});
                    }
                }
                fn.isExternDecl = true;
                prog.functions.push_back(std::move(fn));
                continue;
            }
            auto *local = dynamic_cast<CParser::LocalFunctionDeclContext *>(fd);
            if (!local->block()) continue;  // same-file forward declaration only, nothing to generate

            FunctionDecl fn;
            fn.name = local->IDENT()->getText();
            fn.line = local->getStart()->getLine();
            buildReturnType(local->returnTypeSpec(), fn.returnType, fn.isVoid);
            if (auto *pl = local->paramList()) {
                for (auto *p : pl->param()) {
                    fn.params.push_back(Param{p->IDENT()->getText(), buildType(p->typeSpec())});
                }
            }
            if (local->addr) {
                fn.hasAddress = true;
                fn.address = parsePlainNumber(local->addr, local, "a '@' address");
            }
            fn.body = buildBlock(local->block());
            prog.functions.push_back(std::move(fn));
        } else if (auto *gd = top->globalDecl()) {
            GlobalVar g;
            g.line = gd->getStart()->getLine();
            if (auto *extArr = dynamic_cast<CParser::ExternArrayDeclContext *>(gd)) {
                g.name = extArr->IDENT()->getText();
                g.type = buildType(extArr->typeSpec());
                g.arraySize = static_cast<int>(parsePlainNumber(extArr->size, extArr, "an array size"));
                if (g.arraySize <= 0) err(extArr, "array size must be a positive integer");
                g.isExternDecl = true;
            } else if (auto *extSc = dynamic_cast<CParser::ExternScalarDeclContext *>(gd)) {
                g.name = extSc->IDENT()->getText();
                g.type = buildType(extSc->typeSpec());
                g.isExternDecl = true;
            } else if (auto *arr = dynamic_cast<CParser::GlobalArrayDeclContext *>(gd)) {
                g.name = arr->IDENT()->getText();
                g.type = buildType(arr->typeSpec());
                g.arraySize = static_cast<int>(parsePlainNumber(arr->size, arr, "an array size"));
                if (g.arraySize <= 0) err(arr, "array size must be a positive integer");
                g.arrayInit = buildArrayInit(arr->arrayInit());
                padArrayInit(g.arrayInit, g.arraySize, arr);
                if (arr->addr) {
                    g.hasAddress = true;
                    g.address = parsePlainNumber(arr->addr, arr, "a '@' address");
                }
            } else if (auto *sc = dynamic_cast<CParser::GlobalScalarDeclContext *>(gd)) {
                g.name = sc->IDENT()->getText();
                g.type = buildType(sc->typeSpec());
                if (auto *num = sc->val) {
                    g.hasInit = true;
                    g.initValue = parsePlainNumber(num, sc, "a global initializer");
                    if (sc->sign) g.initValue = -g.initValue;
                }
                if (sc->addr) {
                    g.hasAddress = true;
                    g.address = parsePlainNumber(sc->addr, sc, "a '@' address");
                }
            }
            prog.globals.push_back(std::move(g));
        }
    }
    return prog;
}

}  // namespace rcc
