#pragma once

#include <memory>
#include <string>
#include <vector>

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
};

struct Expr {
    ExprKind kind;
    int line = 0;

    long intValue = 0;        // IntLit
    std::string name;         // Var / Index / Assign (target) / Call (callee)
    std::string op;           // Unary / Binary operator text
    std::unique_ptr<Expr> lhs;  // Binary lhs, Index/indexed-Assign subscript, DerefAssign's pointer
    std::unique_ptr<Expr> rhs;                  // Binary rhs, Assign/DerefAssign rhs, Unary operand
    std::vector<std::unique_ptr<Expr>> args;    // Call arguments
};

using ExprPtr = std::unique_ptr<Expr>;

// --- Statements ---------------------------------------------------------

enum class StmtKind {
    Block,
    If,
    While,
    Return,
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
    std::string declName;                        // LocalDecl
    ExprPtr initOrReturnOrExpr;                   // LocalDecl init / Return value / ExprStmt expr
    int arraySize = 0;                            // LocalDecl: >0 means an array of this many words
    std::vector<long> arrayInit;                  // LocalDecl array: literal initializer (padded with 0s)
    std::string asmText;                           // Asm: raw text between the braces, verbatim
};

using StmtPtr = std::unique_ptr<Stmt>;

// --- Top level ------------------------------------------------------------

struct Param {
    std::string name;
};

struct FunctionDecl {
    std::string name;
    std::vector<Param> params;
    std::unique_ptr<Stmt> body;  // null for a prototype-only declaration
    int line = 0;
    bool hasAddress = false;  // '@' addr given - see IR.h's Function::hasAddress
    long address = 0;
    std::string sourceFile;  // which .c file this came from - see IR.h's Function::sourceFile
};

struct GlobalVar {
    std::string name;
    bool hasInit = false;
    long initValue = 0;
    int line = 0;
    int arraySize = 0;            // >0 means an array of this many words
    std::vector<long> arrayInit;  // array: literal initializer (padded with 0s); unused for scalars
    bool hasAddress = false;       // '@' addr given - see IR.h's Slot::hasAddress
    long address = 0;
    std::string sourceFile;        // which .c file this came from - see IR.h's Slot::sourceFile
};

struct Program {
    std::vector<FunctionDecl> functions;
    std::vector<GlobalVar> globals;
};

}  // namespace pcc
