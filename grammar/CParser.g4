parser grammar CParser;

options { tokenVocab = CLexer; }

// ---------------------------------------------------------------------
// Parser half of pdp-c-compiler's C-subset grammar (see CLexer.g4 for
// the lexer half, and ../README.md for exactly what's in and out of
// scope: single 'int' type, no structs or floats, no preprocessor; a
// char literal is just an int literal spelled as an ASCII character,
// an array's initializer is always a literal list, never a general
// expression, and a pointer is just an int that a '*'/'&' happens to
// be applied to - there's no separate pointer-typed declaration).
// This is a from-scratch grammar sized for that subset, not a full
// ISO C grammar - the same approach ../../pdp-assembler takes for
// as.s with grammar/Pdp7.g4. Keyword/punctuation literals (`'if'`,
// `'('`, ...) are written directly here rather than named in
// CLexer.g4 - ANTLR resolves them against the shared token vocabulary
// automatically; CLexer.g4 only names tokens that need real lexer
// logic (IDENT/NUMBER/CHARLIT/the `asm` block's own mode).
// ---------------------------------------------------------------------

program
    : topLevelDecl* EOF
    ;

topLevelDecl
    : functionDecl
    | globalDecl
    ;

// A trailing ';' instead of a block is a prototype: allowed so a
// function can be declared before its real definition, but it emits
// nothing - BuildAst/CodeGen resolve calls by name against every
// function that has a body, regardless of textual order.
//
// An optional '@' addr pins the function's entry point to an exact
// address - see ../docs/ir.md's "Fixed absolute placement" and
// ../docs/pdp-c-compiler.md's "Placement" section for what that
// address means on each backend, and the real motivation (kernel
// interrupt/trap vectors).
functionDecl
    : 'int' IDENT '(' paramList? ')' ('@' addr=NUMBER)? (block | ';')
    ;

paramList
    : param (',' param)*
    ;

param
    : 'int' IDENT
    ;

// The optional '@' addr on each alternative is the same fixed-address
// placement functionDecl takes - see its comment above.
globalDecl
    : 'int' IDENT '[' size=NUMBER ']' ('=' arrayInit)? ('@' addr=NUMBER)? ';'   # globalArrayDecl
    | 'int' IDENT ('=' sign='-'? val=NUMBER)? ('@' addr=NUMBER)? ';'              # globalScalarDecl
    ;

// Elements are plain (non-negative) literals, or a bare function/
// global name standing for that symbol's own address (the dispatch-
// table idiom a jump table like unix v6/v7's s1.s .sysloc/locsw/locn
// needs: an array of code/data addresses, not values) - no per-
// element sign on the literal form; see ../README.md's "Scope and
// limitations". Resolving a symbol element to its real linked address
// is BuildAst/IRBuilder/the linker's job, same as '&' elsewhere in
// this grammar (condExpr's addrOfExpr) and functionDecl's/
// globalDecl's own '@' addr - this rule only admits the syntax.
arrayInit
    : LBRACE elem+=arrayInitElem (',' elem+=arrayInitElem)* RBRACE
    ;

arrayInitElem
    : NUMBER   # numArrayElem
    | IDENT    # symArrayElem
    ;

block
    : LBRACE statement* RBRACE
    ;

statement
    : block                                                       # blockStmt
    | 'if' '(' expr ')' thenS=statement ('else' elseS=statement)? # ifStmt
    | 'while' '(' expr ')' statement                              # whileStmt
    | 'for' '(' init=simpleStmt? ';' cond=expr? ';' post=simpleStmt? ')' statement # forStmt
    | 'return' expr? ';'                                          # returnStmt
    | ASM_BEGIN text=ASM_TEXT? ASM_END                             # asmStmt
    | simpleStmt ';'                                              # simpleStmtWrapper
    | ';'                                                         # emptyStmt
    ;

simpleStmt
    : 'int' IDENT '[' size=NUMBER ']' ('=' arrayInit)?    # localArrayDecl
    | 'int' IDENT ('=' expr)?                              # localDecl
    | expr                                                  # exprStmt
    ;

expr
    : '*' ptr=condExpr '=' val=expr         # assignDerefExpr
    | IDENT '[' idx=expr ']' '=' val=expr   # assignIndexExpr
    | IDENT '=' expr                         # assignExpr
    | condExpr                                # exprCond
    ;

// Left-recursive precedence chain: the topmost alternative binds
// tightest (ANTLR4 convention - see the ANTLR4 reference's own
// expr-precedence example), down to '||' which binds loosest.
// Bitwise operators (added alongside the arithmetic/logical ones
// above) sit at the same relative spots real C puts them: shifts
// bind tighter than relational comparisons but looser than '+'/'-';
// '&'/'^'/'|' bind looser than equality but tighter than '&&' - see
// ../docs/pdp-c-compiler.md's "Bitwise operators" for what each one
// actually does on each backend (notably: pdp7 has no hardware `or`
// or variable-count shift, so `|` and `<<`/`>>` are synthesized from
// instructions it does have).
condExpr
    : IDENT '(' argList? ')'                          # callExpr
    // Calling through a computed function address (e.g.
    // (table[i])(), (fp)()) rather than a literal function name -
    // the dispatch-table idiom's other half, alongside '&' on a
    // function name (addrOfExpr above). The callee is any
    // parenthesized expr, not just IDENT, same as parenExpr below -
    // ANTLR's lookahead tells the two apart by whether a '(' argList?
    // ')' follows the closing paren. See IRBuilder's buildExpr for
    // this subset's real restriction: no arguments, only ever a bare
    // (expr)() - there's no per-callee ABI slot to put an argument in
    // until the callee is known, which for this alternative it isn't,
    // at compile time.
    | '(' expr ')' '(' argList? ')'                   # callPtrExpr
    | IDENT '[' expr ']'                              # indexExpr
    | IDENT                                           # varExpr
    | NUMBER                                          # numExpr
    | CHARLIT                                         # charExpr
    | '(' expr ')'                                    # parenExpr
    | '*' condExpr                                    # derefExpr
    | '&' condExpr                                    # addrOfExpr
    | op=('-'|'!'|'~') condExpr                        # unaryExpr
    | condExpr op=('*'|'/'|'%') condExpr               # mulExpr
    | condExpr op=('+'|'-') condExpr                   # addExpr
    | condExpr op=('<<'|'>>') condExpr                 # shiftExpr
    | condExpr op=('<'|'<='|'>'|'>=') condExpr         # relExpr
    | condExpr op=('=='|'!=') condExpr                 # eqExpr
    | condExpr '&' condExpr                            # bitAndExpr
    | condExpr '^' condExpr                            # bitXorExpr
    | condExpr '|' condExpr                            # bitOrExpr
    | condExpr '&&' condExpr                           # andExpr
    | condExpr '||' condExpr                           # orExpr
    ;

argList
    : expr (',' expr)*
    ;
