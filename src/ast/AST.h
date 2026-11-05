#pragma once

#include <memory>
#include <string>
#include <vector>

#include "../Type.h"

namespace pcc {

// --- Expressions ------------------------------------------------------

enum class ExprKind {
    IntLit,
    Var,
    Index,        // name[lhs] - array element read
    Assign,       // name = rhs, or (if lhs is set) name[lhs] = rhs
    DerefAssign,  // *lhs = rhs
    Unary,        // op applied to one sub-expression: '-', '!', '*' (deref) or '&' (address-of)
    Binary,       // + - * / % == != < <= > >= && ||
    Call,
    CallPtr,      // (lhs)(args) - callee is a computed address, not a name; see IRBuilder's buildExpr
    Cast,         // (type)rhs
    Sizeof,       // sizeof(type) or sizeof(rhs) - see sizeofIsType
    StringLit,    // "..." - decoded bytes in `name`; see IRBuilder's buildExpr
};

struct Expr {
    ExprKind kind;
    int line = 0;

    long intValue = 0;        // IntLit
    Type type;                 // IntLit (from its suffix); Cast/Sizeof (the operand/measured type)
    bool sizeofIsType = false;  // Sizeof: true -> `type` is what's measured, false -> `rhs` is (unevaluated)
    std::string name;         // Var / Index / Assign (target) / Call (callee) / StringLit (decoded bytes)
    std::string op;           // Unary / Binary operator text
    std::unique_ptr<Expr> lhs;  // Binary lhs, Index/indexed-Assign subscript, DerefAssign's pointer,
                                 // CallPtr's callee expression
    std::unique_ptr<Expr> rhs;                  // Binary rhs, Assign/DerefAssign rhs, Unary operand,
                                                  // Cast operand, Sizeof's measured expression
    std::vector<std::unique_ptr<Expr>> args;    // Call / CallPtr arguments
};

using ExprPtr = std::unique_ptr<Expr>;

// --- Statements ---------------------------------------------------------

// One element of an array initializer list - either a plain literal,
// or a bare name standing for that symbol's (a function's or a
// global's) own address, the dispatch-table idiom (e.g. "int t[] =
// {f, g};") - see grammar/CParser.g4's arrayInitElem and IRBuilder.cpp,
// which resolves `symbolName` to a Slot or a Function the same way it
// resolves a bare "&name" expression.
struct ArrayInitElem {
    bool isSymbol = false;
    long value = 0;            // literal value, when !isSymbol
    std::string symbolName;     // identifier text, when isSymbol
};

enum class StmtKind {
    Block,
    If,
    While,
    Return,
    Break,     // see Stmt's own comment on While's loopBody/postStmt for what it targets
    Continue,  // see Stmt's own comment on While's loopBody/postStmt for what it targets
    LocalDecl,
    ExprStmt,
    Empty,
    Asm,  // asm { ... } - raw target text, emitted verbatim; see AST.h's Stmt::asmText
};

struct Stmt {
    StmtKind kind;
    int line = 0;

    std::vector<std::unique_ptr<Stmt>> body;   // Block
    ExprPtr cond;                                // If / While
    std::unique_ptr<Stmt> thenStmt, elseStmt;    // If
    std::unique_ptr<Stmt> loopBody;              // While
    // A `for(init;cond;post)` is desugared into `{ init; while(cond)
    // ... }` right here in BuildAst.cpp - but `post` is kept as its
    // OWN field, never folded into loopBody, specifically so
    // `continue` inside the loop body can skip the rest of loopBody
    // while still running post before the condition is retested (a
    // real While has no postStmt at all - null here - so its own
    // `continue` just jumps straight back to the condition check,
    // same as always). Null for a real `while`.
    std::unique_ptr<Stmt> postStmt;              // While (for-loop desugaring only)
    std::string declName;                        // LocalDecl
    Type type;                                     // LocalDecl: declared type (scalar, or array element type)
    ExprPtr initOrReturnOrExpr;                   // LocalDecl init / Return value / ExprStmt expr
    int arraySize = 0;                            // LocalDecl: >0 means an array of this many words
    std::vector<ArrayInitElem> arrayInit;         // LocalDecl array: initializer (padded with 0s)
    std::string asmText;                           // Asm: raw text between the braces, verbatim
};

using StmtPtr = std::unique_ptr<Stmt>;

// --- Top level ------------------------------------------------------------

struct Param {
    std::string name;
    Type type;
};

struct FunctionDecl {
    std::string name;
    Type returnType;
    std::vector<Param> params;
    std::unique_ptr<Stmt> body;  // null for a prototype-only declaration
    int line = 0;
    bool hasAddress = false;  // '@' addr given - see IR.h's Function::hasAddress
    long address = 0;
    std::string sourceFile;  // which .c file this came from - see IR.h's Function::sourceFile
};

struct GlobalVar {
    std::string name;
    Type type;                     // scalar type, or array element type
    bool hasInit = false;
    long initValue = 0;
    int line = 0;
    int arraySize = 0;            // >0 means an array of this many words
    std::vector<ArrayInitElem> arrayInit;  // array: initializer (padded with 0s); unused for scalars
    bool hasAddress = false;       // '@' addr given - see IR.h's Slot::hasAddress
    long address = 0;
    std::string sourceFile;        // which .c file this came from - see IR.h's Slot::sourceFile
};

struct Program {
    std::vector<FunctionDecl> functions;
    std::vector<GlobalVar> globals;
};

}  // namespace pcc
