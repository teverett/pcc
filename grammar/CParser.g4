parser grammar CParser;

options { tokenVocab = CLexer; }

// ---------------------------------------------------------------------
// Parser half of rcc's C-subset grammar (see CLexer.g4 for
// the lexer half, and ../README.md for exactly what's in and out of
// scope: char/short/int/long/long long, each signed or unsigned, each
// optionally pointer-typed (typeSpec's own trailing '*'s - see
// ../src/Type.h's Type::ptrDepth), no structs or floats, no
// preprocessor; an array's initializer is always a literal list, never
// a general expression). This is a from-scratch grammar sized for that
// subset, not a full ISO C grammar - the same approach ../../pdp-assembler
// takes for as.s with grammar/Pdp7.g4. Keyword/punctuation literals (`'if'`,
// `'('`, ...) are written directly here rather than named in
// CLexer.g4 - ANTLR resolves them against the shared token vocabulary
// automatically; CLexer.g4 only names tokens that need real lexer
// logic (IDENT/NUMBER/CHARLIT/the `asm` block's own mode).
// ---------------------------------------------------------------------

program
    : topLevelDecl* EOF
    ;

// A declared type - every 'int' this grammar used to hardcode (in a
// function/param/global/local declaration, or a cast/sizeof's
// parenthesized operand) is one of these now. A trailing run of '*'s
// (zero or more) makes it a pointer - 'int*' one level, 'int**' two,
// etc; see ../src/Type.h's own Type::ptrDepth. No ambiguity with
// mulExpr/parenExpr: baseTypeSpec always starts with one of
// char/short/int/long/signed/unsigned, none of which can start an
// expr, so ANTLR tells a cast/sizeof's parenthesized type apart from a
// parenthesized expression by that first token alone, same as before
// this rule grew its own trailing '*'s.
typeSpec
    : baseTypeSpec '*'*
    ;

// BuildAst turns this into a rcc::Type (see ../src/Type.h); a bare
// 'signed'/'unsigned' with no following width keyword means int, same
// as real C.
baseTypeSpec
    : ('signed' | 'unsigned')? 'char'                        # charType
    | ('signed' | 'unsigned')? 'short' 'int'?                 # shortType
    | ('signed' | 'unsigned')? 'int'                          # intType
    | ('signed' | 'unsigned')? 'long' 'long' 'int'?           # longLongType
    | ('signed' | 'unsigned')? 'long' 'int'?                  # longType
    | ('signed' | 'unsigned')                                  # signedOrUnsignedType
    ;

topLevelDecl
    : functionDecl
    | globalDecl
    ;

// A trailing ';' instead of a block, with no leading 'extern', is a
// same-file forward declaration: allowed so a function can be
// declared before its real definition textually, but it emits
// nothing and has no effect on resolution - BuildAst discards it
// outright (see BuildAst.cpp's buildProgram); every function in this
// file is already registered by name, regardless of textual order,
// before any body is built. An 'extern' declaration (always
// bodyless) is different: it's the only way to tell IRBuilder a
// name used only via '&'/an array-initializer element in this file
// (never called, so resolveFunc's own call-site fallback never
// fires) is a FUNCTION defined in another file, not data - see
// IRBuilder.cpp's own header comment on why this is needed at all now
// (neither direction can be silently guessed any more).
//
// An optional '@' addr pins the function's entry point to an exact
// address - see ../docs/ir.md's "Fixed absolute placement" and
// ../docs/language.md's "Placement" section for what that
// address means on each backend, and the real motivation (kernel
// interrupt/trap vectors). Not meaningful on an 'extern' declaration
// (that address lives in whichever file really defines it), so not
// offered on that alternative.
functionDecl
    : 'extern' returnTypeSpec IDENT '(' paramList? ')' ';'                         # externFunctionDecl
    | returnTypeSpec IDENT '(' paramList? ')' ('@' addr=NUMBER)? (block | ';')     # localFunctionDecl
    ;

// A function's return type is either an ordinary typeSpec or 'void'
// ("no value") - 'void' is deliberately not a baseTypeSpec alternative,
// so it can never appear anywhere else typeSpec is used (a variable/
// param/cast/sizeof operand), only here.
returnTypeSpec
    : typeSpec
    | 'void'
    ;

paramList
    : param (',' param)*
    ;

param
    : typeSpec IDENT
    ;

// The optional '@' addr on each local alternative is the same fixed-
// address placement functionDecl takes - see its comment above. An
// 'extern' global declaration (always no initializer, no '@') is the
// data-side counterpart of an 'extern' function declaration above:
// the only way to tell IRBuilder a name used only via '&'/an array-
// initializer element in this file is DATA defined in another file,
// not a function.
globalDecl
    : 'extern' typeSpec IDENT '[' size=NUMBER ']' ';'                            # externArrayDecl
    | 'extern' typeSpec IDENT ';'                                                 # externScalarDecl
    | typeSpec IDENT '[' size=NUMBER ']' ('=' arrayInit)? ('@' addr=NUMBER)? ';'   # globalArrayDecl
    | typeSpec IDENT ('=' sign='-'? val=NUMBER)? ('@' addr=NUMBER)? ';'              # globalScalarDecl
    ;

// Elements are plain literals, optionally negated, or a bare function/
// global name standing for that symbol's own address (the dispatch-
// table idiom a jump table like unix v6/v7's s1.s .sysloc/locsw/locn
// needs: an array of code/data addresses, not values). Resolving a
// symbol element to its real linked address
// is BuildAst/IRBuilder/the linker's job, same as '&' elsewhere in
// this grammar (condExpr's addrOfExpr) and functionDecl's/
// globalDecl's own '@' addr - this rule only admits the syntax.
arrayInit
    : LBRACE elem+=arrayInitElem (',' elem+=arrayInitElem)* RBRACE
    ;

arrayInitElem
    : sign='-'? NUMBER   # numArrayElem
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
    | 'switch' '(' expr ')' LBRACE switchCase* RBRACE             # switchStmt
    | 'return' expr? ';'                                          # returnStmt
    | 'break' ';'                                                 # breakStmt
    | 'continue' ';'                                              # continueStmt
    | 'goto' IDENT ';'                                            # gotoStmt
    // A label attaches to exactly the one statement that follows its
    // ':' (real C's own rule - the label isn't a block of its own),
    // so this recurses back into `statement`, letting `a: b: stmt;`
    // chain naturally. No ambiguity with assignExpr/exprStmt (also
    // IDENT-first): the very next token (':' vs '='/';'/etc) tells
    // them apart.
    | IDENT ':' statement                                         # labelStmt
    | ASM_BEGIN text=ASM_TEXT? ASM_END                             # asmStmt
    | simpleStmt ';'                                              # simpleStmtWrapper
    | ';'                                                         # emptyStmt
    ;

simpleStmt
    : typeSpec IDENT '[' size=NUMBER ']' ('=' arrayInit)?    # localArrayDecl
    | typeSpec IDENT ('=' expr)?                              # localDecl
    | expr                                                  # exprStmt
    ;

// A case label's value is a plain (optionally negated) integer literal,
// not a general constant-expression - the same deliberately-minimal
// idiom arrayInitElem already uses for its own element literals (see
// arrayInitElem above). `statement*` (not a required `block`) is what
// gives case/default bodies real C fallthrough: nothing here stops
// control reaching the next switchCase's label, same as the labels
// themselves impose no scope of their own (BuildAst/IRBuilder treat the
// whole switch body as a single scope, like block's own statement*).
switchCase
    : 'case' sign='-'? val=NUMBER ':' statement*   # caseLabel
    | 'default' ':' statement*                     # defaultLabel
    ;

// op defaults to plain '=' (an ordinary assignment); any of the ten
// compound-assignment operators instead means "combine with the
// current value first" - see BuildAst.cpp's own handling of this
// token for exactly which AST node each one builds (a bare '=' keeps
// building today's plain Assign/DerefAssign, unchanged; a compound
// operator builds CompoundAssign/CompoundDerefAssign instead, so the
// target's own index/pointer sub-expression is only ever evaluated
// once - see ../docs/ir.md's own note on this).
assignOp
    : '=' | '+=' | '-=' | '*=' | '/=' | '%=' | '&=' | '^=' | '|=' | '<<=' | '>>='
    ;

expr
    : '*' ptr=condExpr op=assignOp val=expr         # assignDerefExpr
    | IDENT '[' idx=expr ']' op=assignOp val=expr   # assignIndexExpr
    | IDENT op=assignOp val=expr                     # assignExpr
    | condExpr                                        # exprCond
    ;

// Left-recursive precedence chain: the topmost alternative binds
// tightest (ANTLR4 convention - see the ANTLR4 reference's own
// expr-precedence example), down to '||' which binds loosest.
// Bitwise operators (added alongside the arithmetic/logical ones
// above) sit at the same relative spots real C puts them: shifts
// bind tighter than relational comparisons but looser than '+'/'-';
// '&'/'^'/'|' bind looser than equality but tighter than '&&' - see
// ../docs/language.md's "Bitwise operators" for what each one
// actually does on each backend (notably: pdp7 has no hardware `or`
// or variable-count shift, so `|` and `<<`/`>>` are synthesized from
// instructions it does have).
condExpr
    // Postfix '++'/'--': same three assignable-target shapes as
    // assignExpr/assignIndexExpr/assignDerefExpr above (BuildAst
    // enforces this - plain IDENT, IDENT[idx], or *condExpr - there's
    // no grammar rule for "any arbitrary assignable condExpr", same
    // reasoning as assignment's own three explicit shapes). Placed at
    // the same tightest tier as the primaries below, matching real C's
    // own postfix-expression precedence (binds tighter than the
    // prefix form a few lines down).
    : IDENT op=('++'|'--')                            # postIncDecVarExpr
    | IDENT '[' idx=expr ']' op=('++'|'--')           # postIncDecIndexExpr
    | '*' ptr=condExpr op=('++'|'--')                 # postIncDecDerefExpr
    | IDENT '(' argList? ')'                          # callExpr
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
    | STRINGLIT                                       # stringLitExpr
    | '(' expr ')'                                    # parenExpr
    // No ambiguity with parenExpr/callPtrExpr above: typeSpec always
    // starts with one of char/short/int/long/signed/unsigned, none of
    // which can start an expr, so ANTLR tells these apart by their
    // first token after '('.
    | '(' typeSpec ')' condExpr                       # castExpr
    | 'sizeof' '(' typeSpec ')'                       # sizeofTypeExpr
    | 'sizeof' '(' expr ')'                           # sizeofExprExpr
    | '*' condExpr                                    # derefExpr
    | '&' condExpr                                    # addrOfExpr
    | op=('-'|'!'|'~') condExpr                        # unaryExpr
    // Prefix '++'/'--' - same three assignable-target shapes as the
    // postfix form above, operator first; sits at this same tier as
    // the other prefix unary operators, matching real C.
    | op=('++'|'--') IDENT                            # preIncDecVarExpr
    | op=('++'|'--') IDENT '[' idx=expr ']'           # preIncDecIndexExpr
    | op=('++'|'--') '*' ptr=condExpr                 # preIncDecDerefExpr
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
    // Loosest of all (same placement real C uses: looser than '||',
    // but above assignment - reached only through expr's exprCond
    // alternative or one of the `expr`-typed primaries above, e.g.
    // inside a call argument or an array index). Right-associative for
    // the chained `a ? b : c ? d : e` case (meaning `a ? b : (c ? d :
    // e)`): 'then' is a full expr (so an assignment is allowed there
    // unparenthesized, same as real C's own grammar), 'else' recurses
    // back into condExpr to pick up another ternary (or anything
    // tighter) without needing its own parens.
    | <assoc=right> cond=condExpr '?' then=expr ':' els=condExpr      # ternaryExpr
    ;

argList
    : expr (',' expr)*
    ;
