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
    // name op= rhs, or (if lhs is set) name[lhs] op= rhs - `op` names the
    // underlying binary operator (the compound token with its trailing
    // '=' stripped, e.g. "+=" -> "+"); `yieldOld` is true only for a
    // postfix '++'/'--' (built as op="+"/"-", rhs=IntLit(1)) - see
    // IRBuilder.cpp's own note on why this is a real IR node rather than
    // a `name = name op rhs` desugar: the target's index/pointer
    // sub-expression must only be evaluated once, and returning the
    // *old* value (postfix) needs the value to still be available right
    // after the load, before the store.
    CompoundAssign,
    CompoundDerefAssign,  // *lhs op= rhs - same as CompoundAssign, deref target
    Unary,        // op applied to one sub-expression: '-', '!', '*' (deref) or '&' (address-of)
    Binary,       // + - * / % == != < <= > >= && ||
    Call,
    CallPtr,      // (lhs)(args) - callee is a computed address, not a name; see IRBuilder's buildExpr
    Cast,         // (type)rhs
    Sizeof,       // sizeof(type) or sizeof(rhs) - see sizeofIsType
    StringLit,    // "..." - decoded bytes in `name`; see IRBuilder's buildExpr
    Ternary,      // lhs ? rhs : elseExpr
};

struct Expr {
    ExprKind kind;
    int line = 0;

    long intValue = 0;        // IntLit
    Type type;                 // IntLit (from its suffix); Cast/Sizeof (the operand/measured type)
    bool sizeofIsType = false;  // Sizeof: true -> `type` is what's measured, false -> `rhs` is (unevaluated)
    bool yieldOld = false;      // CompoundAssign/CompoundDerefAssign: true only for postfix '++'/'--'
    std::string name;         // Var / Index / Assign (target) / Call (callee) / StringLit (decoded bytes)
    std::string op;           // Unary / Binary / CompoundAssign(DerefAssign) operator text
    std::unique_ptr<Expr> lhs;  // Binary lhs, Index/indexed-Assign(CompoundAssign) subscript,
                                 // DerefAssign/CompoundDerefAssign's pointer, CallPtr's callee expression,
                                 // Ternary's condition
    std::unique_ptr<Expr> rhs;                  // Binary rhs, Assign/DerefAssign/CompoundAssign(DerefAssign)
                                                  // value, Unary operand, Cast operand, Sizeof's measured
                                                  // expression, Ternary's then-branch
    std::unique_ptr<Expr> elseExpr;              // Ternary's else-branch
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
    std::unique_ptr<Stmt> body;  // null for a prototype-only declaration, or an 'extern' one
    int line = 0;
    bool hasAddress = false;  // '@' addr given - see IR.h's Function::hasAddress
    long address = 0;
    std::string sourceFile;  // which .c file this came from - see IR.h's Function::sourceFile

    // True for an 'extern' declaration (always bodyless): this name
    // is a function defined in another file - see IR.h's
    // Function::isExternal and IRBuilder.cpp's own header comment on
    // why this now has to be stated explicitly rather than guessed.
    bool isExternDecl = false;
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

    // True for an 'extern' declaration (always uninitialized): this
    // name is data defined in another file - see Slot::isExternal
    // and IRBuilder.cpp's own header comment on why this now has to
    // be stated explicitly rather than guessed.
    bool isExternDecl = false;
};

struct Program {
    std::vector<FunctionDecl> functions;
    std::vector<GlobalVar> globals;
};

}  // namespace pcc
