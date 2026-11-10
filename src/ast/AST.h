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
    Index,        // name[lhs]
    Assign,       // name = rhs, or name[lhs] = rhs if lhs is set
    DerefAssign,  // *lhs = rhs
    // name op= rhs (or name[lhs] op= rhs); `op` is the operator with its
    // trailing '=' stripped. Kept as its own node, not desugared, because the
    // index/pointer target must be evaluated once and postfix '++'/'--' needs
    // the old value available between the load and the store.
    CompoundAssign,
    CompoundDerefAssign,  // *lhs op= rhs
    Unary,        // '-', '!', '*' (deref), '&' (address-of)
    Binary,       // + - * / % == != < <= > >= && ||
    Call,
    CallPtr,      // (lhs)(args): callee is a computed address, not a name
    Cast,         // (type)rhs
    Sizeof,       // sizeof(type) or sizeof(rhs), per sizeofIsType
    StringLit,    // decoded bytes stored in `name`
    Ternary,      // lhs ? rhs : elseExpr
};

struct Expr {
    ExprKind kind;
    int line = 0;

    long intValue = 0;        // IntLit
    Type type;                 // IntLit's suffix type; Cast/Sizeof's operand/measured type
    bool sizeofIsType = false;  // Sizeof: true -> `type` is measured, false -> `rhs` is (unevaluated)
    bool yieldOld = false;      // CompoundAssign(DerefAssign): true only for postfix '++'/'--'
    std::string name;         // Var / Index / Assign target / Call callee / StringLit bytes
    std::string op;           // Unary / Binary / CompoundAssign(DerefAssign) operator text
    std::unique_ptr<Expr> lhs;  // Binary lhs, Index/Assign(CompoundAssign) subscript, DerefAssign
                                 // (CompoundDerefAssign) pointer, CallPtr callee, Ternary condition
    std::unique_ptr<Expr> rhs;                  // Binary rhs, Assign(DerefAssign/CompoundAssign) value,
                                                  // Unary/Cast operand, Sizeof expression, Ternary then-branch
    std::unique_ptr<Expr> elseExpr;              // Ternary else-branch
    std::vector<std::unique_ptr<Expr>> args;    // Call / CallPtr arguments
};

using ExprPtr = std::unique_ptr<Expr>;

// --- Statements ---------------------------------------------------------

// Array initializer element: a literal value, or a bare name standing for
// that function/global's own address (the dispatch-table idiom).
struct ArrayInitElem {
    bool isSymbol = false;
    long value = 0;            // literal value, when !isSymbol
    std::string symbolName;     // identifier text, when isSymbol
};

enum class StmtKind {
    Block,
    If,
    While,
    Switch,
    Return,
    Break,
    Continue,
    LocalDecl,
    ExprStmt,
    Empty,
    Asm,  // raw target text, emitted verbatim
    Label,  // `name: thenStmt` - see Stmt::declName/thenStmt
    Goto,   // `goto name;` - see Stmt::declName
};

struct Stmt;

// One `case N:`/`default:` label and the statements that follow it, up to
// (not including) the next label - see Stmt::cases. Real C fallthrough:
// nothing here marks where a case's statements end, so control simply
// keeps going into the next label's statements unless a `break` (or
// `return`) stops it first, same as the source order IRBuilder sees them in.
struct SwitchCase {
    bool isDefault = false;
    long value = 0;  // case constant, meaningful when !isDefault
    std::vector<std::unique_ptr<Stmt>> body;
};

struct Stmt {
    StmtKind kind;
    int line = 0;

    std::vector<std::unique_ptr<Stmt>> body;   // Block
    ExprPtr cond;                                // If / While / Switch (the switched-on expression)
    std::unique_ptr<Stmt> thenStmt, elseStmt;    // If; thenStmt also doubles as Label's wrapped statement
    std::unique_ptr<Stmt> loopBody;              // While
    // `for(init;cond;post)` desugars to `{ init; while(cond) ... }` with
    // `post` kept separate from loopBody so `continue` still runs it before
    // retesting cond. Null for a real `while`.
    std::unique_ptr<Stmt> postStmt;              // While (for-loop desugaring only)
    std::vector<SwitchCase> cases;                // Switch: case/default labels, in source order
    std::string declName;                        // LocalDecl name; Label's own name; Goto's target name
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
    std::unique_ptr<Stmt> body;  // null for a prototype-only or 'extern' declaration
    int line = 0;
    bool hasAddress = false;  // '@' address given explicitly
    long address = 0;
    std::string sourceFile;  // originating .c file

    bool isExternDecl = false;  // 'extern': defined in another file
};

struct GlobalVar {
    std::string name;
    Type type;                     // scalar type, or array element type
    bool hasInit = false;
    long initValue = 0;
    int line = 0;
    int arraySize = 0;            // >0 means an array of this many words
    std::vector<ArrayInitElem> arrayInit;  // array initializer (padded with 0s); unused for scalars
    bool hasAddress = false;       // '@' address given explicitly
    long address = 0;
    std::string sourceFile;        // originating .c file

    bool isExternDecl = false;  // 'extern': data defined in another file
};

struct Program {
    std::vector<FunctionDecl> functions;
    std::vector<GlobalVar> globals;
};

}  // namespace pcc
