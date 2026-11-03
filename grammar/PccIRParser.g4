parser grammar PccIRParser;

options { tokenVocab = PccIRLexer; }

// ---------------------------------------------------------------------
// Parser half of the grammar for IRPrinter.cpp's own output (see
// PccIRLexer.g4's header comment for the split-grammar/`asm {...}`
// reason, and ../src/ir/IRValidator.h for where this gets used).
//
// No operator-precedence table: IRPrinter.cpp always fully
// parenthesizes every Binary node (`"(" lhs " " op " " rhs ")"`) and
// every Unary/deref node (`op "(" rhs ")"`), so every expression form
// here is syntactically distinct with no ambiguity to resolve by
// precedence - this grammar only has to accept the exact shapes that
// printer ever writes.
// ---------------------------------------------------------------------

program
    : GLOBALS_HEADER globalDecl* FUNCTIONS_HEADER funcDecl* EOF
    ;

// "int name#id;" / "int name#id = 5;" / "int name#id[3];" /
// "int name#id[3] = {1, 2, 3};" - see IRPrinter.cpp's own globals
// loop. A global's scalar initializer is always a plain literal (the
// language requires a compile-time constant there - see
// IRBuilder.h), never a general expr the way a LocalDecl's can be.
globalDecl
    : INT_KW ref (LBRACK NUMBER RBRACK (ASSIGN initList)? | (ASSIGN NUMBER)?) SEMI
    ;

initList
    : LBRACE (initElem (COMMA initElem)*)? RBRACE
    ;

// A plain literal, or a symbol reference standing for that Slot's or
// Function's own address (see IR.h's ArrayInitElem) - identical shape
// to varExpr's bare `ref`, just not wrapped as a full `expr` since an
// initList element is never anything else (no operators, no calls).
initElem
    : NUMBER
    | ref
    ;

// "int name#id(int p#id, ...)\n{ ... }\n" - see IRPrinter.cpp's own
// functions loop.
funcDecl
    : INT_KW ref LPAREN paramList? RPAREN block
    ;

paramList
    : param (COMMA param)*
    ;

param
    : INT_KW ref
    ;

block
    : LBRACE stmt* RBRACE
    ;

// One alternative per ir::StmtKind (see IR.h); shapes match
// IRPrinter.cpp's printStmt() exactly, including that `local`'s array
// form always carries an `initList`, never a general `= expr` the
// way its scalar form can (see IR.h's ArrayInitElem).
stmt
    : block                                                                        # blockStmt
    | IF_KW LPAREN expr RPAREN stmt (ELSE_KW stmt)?                                 # ifStmt
    | WHILE_KW LPAREN expr RPAREN stmt (POST_KW stmt)?                               # whileStmt
    | RETURN_KW expr? SEMI                                                          # returnStmt
    | BREAK_KW SEMI                                                                 # breakStmt
    | CONTINUE_KW SEMI                                                              # continueStmt
    | LOCAL_KW ref (LBRACK NUMBER RBRACK (ASSIGN initList)? | (ASSIGN expr)?) SEMI  # localDeclStmt
    | ASM_BEGIN ASM_TEXT ASM_END                                                    # asmStmt
    | SEMI                                                                          # emptyStmt
    | expr SEMI                                                                     # exprStmt
    ;

// "name#id" - a Slot or Function reference (IRPrinter.cpp's
// slotRef()/funcRef(): identical shape for both, so one rule covers
// every use - a global/local/param Slot or a Function).
ref
    : IDENT HASH NUMBER
    ;

// Every alternative's shape is lifted straight from IRPrinter.cpp's
// printExpr() - see the comment on each ExprKind case there for the
// exact spelling this has to accept.
expr
    : NUMBER                                                      # intLitExpr
    | ref LBRACK expr RBRACK (ASSIGN expr)?                        # indexExpr        // Index, or indexed Assign
    | ref ASSIGN expr                                              # assignExpr
    | STAR LPAREN expr RPAREN (ASSIGN expr)?                       # starExpr         // Unary "*" (deref read), or DerefAssign
    | (MINUS | BANG | TILDE | AMP) LPAREN expr RPAREN               # unaryExpr
    | LPAREN expr binOp expr RPAREN                                 # binaryExpr
    | ref LPAREN argList? RPAREN                                    # callExpr
    | LPAREN expr RPAREN LPAREN RPAREN                              # callPtrExpr      // CallPtr - always 0 args, see IR.h
    | IDENT LPAREN RPAREN                                           # intrinsicExpr    // "_ion()" / "_iof()"
    | ref                                                           # varExpr
    ;

argList
    : expr (COMMA expr)*
    ;

binOp
    : PLUS | MINUS | STAR | SLASH | PERCENT | SHL | SHR
    | EQ | NE | LT | LE | GT | GE | AMP | CARET | PIPE | ANDAND | OROR
    ;
