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

// IRPrinter.cpp's own canonical spelling of a pcc::Type (see
// typeSpecText() there) - always the shortest real-C spelling
// ("int"/"unsigned int"/"char"/.../"long long"/"unsigned long long"),
// never the source grammar's fuller combinatorial typeSpec (e.g.
// "short int"/"signed short"), plus a trailing run of '*'s for a
// pointer (see ../src/Type.h's Type::ptrDepth).
typeSpec
    : (UNSIGNED_KW? CHAR_KW
      | UNSIGNED_KW? SHORT_KW
      | UNSIGNED_KW? INT_KW
      | UNSIGNED_KW? LONG_KW LONG_KW
      | UNSIGNED_KW? LONG_KW
      ) STAR*
    ;

// "int name#id;" / "int name#id = 5;" / "int name#id[3];" /
// "int name#id[3] = {1, 2, 3};" - see IRPrinter.cpp's own globals
// loop. A global's scalar initializer is always a plain literal (the
// language requires a compile-time constant there - see
// IRBuilder.h), never a general expr the way a LocalDecl's can be.
globalDecl
    : typeSpec ref (LBRACK NUMBER RBRACK (ASSIGN initList)? | (ASSIGN NUMBER)?) SEMI
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
    : typeSpec ref LPAREN paramList? RPAREN block
    ;

paramList
    : param (COMMA param)*
    ;

param
    : typeSpec ref
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
    | LOCAL_KW typeSpec ref (LBRACK NUMBER RBRACK (ASSIGN initList)? | (ASSIGN expr)?) SEMI  # localDeclStmt
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
// The ten compound-assignment operator spellings (IRPrinter.cpp's
// `e.op + "="`), shared by compoundIndexExpr/compoundAssignExpr/
// compoundDerefExpr below.
compoundOp
    : PLUSEQ | MINUSEQ | STAREQ | SLASHEQ | PERCENTEQ | AMPEQ | CARETEQ | PIPEEQ | SHLEQ | SHREQ
    ;

expr
    : NUMBER                                                      # intLitExpr
    // CompoundAssign/CompoundDerefAssign (IRPrinter.cpp's own case) -
    // OLDVAL_KW present only when `e.yieldOld` is true (a postfix
    // '++'/'--'); these three must be tried before indexExpr/
    // assignExpr/starExpr below, which would otherwise also match
    // their own leading `ref`/`STAR` before ever reaching the
    // compound operator.
    | OLDVAL_KW? ref LBRACK expr RBRACK compoundOp expr             # compoundIndexExpr
    | OLDVAL_KW? ref compoundOp expr                                # compoundAssignExpr
    | OLDVAL_KW? STAR LPAREN expr RPAREN compoundOp expr            # compoundDerefExpr
    | ref LBRACK expr RBRACK (ASSIGN expr)?                        # indexExpr        // Index, or indexed Assign
    | ref ASSIGN expr                                              # assignExpr
    | STAR LPAREN expr RPAREN (ASSIGN expr)?                       # starExpr         // Unary "*" (deref read), or DerefAssign
    | (MINUS | BANG | TILDE | AMP) LPAREN expr RPAREN               # unaryExpr
    | LPAREN expr binOp expr RPAREN                                 # binaryExpr
    // ExprKind::Ternary (IRPrinter.cpp's own case: "(" cond " ? " then
    // " : " els ")").
    | LPAREN expr QUESTION expr COLON expr RPAREN                   # ternaryExpr
    // ExprKind::Convert - an implicit or explicit type conversion
    // (IRPrinter.cpp's printExpr: "(" typeSpecText(e.type) ")(" ... ")").
    // No ambiguity with binaryExpr/callPtrExpr above: typeSpec always
    // starts with CHAR_KW/SHORT_KW/INT_KW/LONG_KW/UNSIGNED_KW, none of
    // which can start an expr.
    | LPAREN typeSpec RPAREN LPAREN expr RPAREN                     # convertExpr
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
