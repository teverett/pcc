lexer grammar RccIRLexer;

// ---------------------------------------------------------------------
// Lexer half of the grammar for IRPrinter.cpp's own output format (see
// ../src/ir/IRPrinter.h/.cpp) - the text --dump-ir writes, and every
// program's own printed form that ir/IRValidator.h parses against
// this grammar on every compile (see main.cpp's call site) as a
// self-check that the printer and this grammar haven't drifted apart.
//
// Split from a single combined grammar for the same reason as
// CLexer.g4/CParser.g4: `asm { ... }` (an IR Stmt's raw target text,
// printed verbatim - see IRPrinter.cpp's StmtKind::Asm case) needs a
// real lexer mode to capture it, and ANTLR4 lexer modes aren't
// available in a combined parser+lexer grammar.
//
// Deliberately NOT a general-purpose grammar: it describes exactly
// what IRPrinter.cpp emits, nothing more - e.g. every binary/unary
// expression is always fully parenthesized by the printer, so this
// grammar needs no operator-precedence table at all (see
// RccIRParser.g4's own comment).
// ---------------------------------------------------------------------

// The two fixed section-header lines printIR() always writes (see
// IRPrinter.cpp). Matched as whole literals - longer than a bare `;`
// comment line would be, so ANTLR's own maximal-munch rule always
// prefers these over treating the line as a comment, with no need for
// a general `;`-starts-a-comment rule here at all (unlike
// grammar/RccAsm.g4 - this format uses `;` only as a statement
// terminator, never for comments).
GLOBALS_HEADER   : '; --- globals ---' ;
FUNCTIONS_HEADER : '; --- functions ---' ;

// Keyword/punctuation tokens RccIRParser.g4 uses as bare string
// literals - an explicit rule here because split lexer+parser
// grammars (unlike a combined one) can't auto-create them for the
// parser; see CLexer.g4's own comment for the same point.
INT_KW    : 'int' ;
CHAR_KW   : 'char' ;
SHORT_KW  : 'short' ;
LONG_KW   : 'long' ;
UNSIGNED_KW : 'unsigned' ;
VOID_KW   : 'void' ;
STRUCT_KW : 'struct' ;
LOCAL_KW  : 'local' ;
IF_KW     : 'if' ;
ELSE_KW   : 'else' ;
WHILE_KW  : 'while' ;
RETURN_KW : 'return' ;
BREAK_KW  : 'break' ;
CONTINUE_KW : 'continue' ;
GOTO_KW   : 'goto' ;
POST_KW   : 'post' ;
// Printed immediately before a CompoundAssign/CompoundDerefAssign
// whose `yieldOld` is true (a postfix '++'/'--' - see IRPrinter.cpp's
// own CompoundAssign/CompoundDerefAssign case) - a distinct keyword
// from POST_KW above, which already means something else entirely
// (While's own postStmt).
OLDVAL_KW : 'oldval' ;
LPAREN : '(' ;
RPAREN : ')' ;
LBRACK : '[' ;
RBRACK : ']' ;
LBRACE : '{' ;
RBRACE : '}' ;
SEMI   : ';' ;
COMMA  : ',' ;
ASSIGN : '=' ;
HASH   : '#' ;
STAR   : '*' ;
AMP    : '&' ;
MINUS  : '-' ;
BANG   : '!' ;
TILDE  : '~' ;
SLASH  : '/' ;
PERCENT: '%' ;
PLUS   : '+' ;
SHL    : '<<' ;
SHR    : '>>' ;
LE     : '<=' ;
GE     : '>=' ;
LT     : '<' ;
GT     : '>' ;
EQ     : '==' ;
NE     : '!=' ;
CARET  : '^' ;
PIPE   : '|' ;
ANDAND : '&&' ;
OROR   : '||' ;
// Compound-assignment operator spellings (IRPrinter.cpp's own
// CompoundAssign/CompoundDerefAssign case: `e.op + "="`) and the
// ternary operator's own two punctuation marks.
PLUSEQ    : '+=' ;
MINUSEQ   : '-=' ;
STAREQ    : '*=' ;
SLASHEQ   : '/=' ;
PERCENTEQ : '%=' ;
AMPEQ     : '&=' ;
CARETEQ   : '^=' ;
PIPEEQ    : '|=' ;
SHLEQ     : '<<=' ;
SHREQ     : '>>=' ;
QUESTION  : '?' ;
COLON     : ':' ;

IDENT  : [A-Za-z_][A-Za-z0-9_]* ;
// A leading '-' is part of this token (not a separate MINUS) only
// when immediately followed by a digit, e.g. a negative IntLit/array-
// initializer value (`out << e.intValue;`, `out << arrayInit[i];` in
// IRPrinter.cpp print it exactly this way - a bare sign with no space
// and no surrounding parens). Every operator use of '-' instead has a
// space, a '(', or another operator character right after it, so it
// never matches this rule - see RccIRParser.g4's own comment.
NUMBER : '-'? [0-9]+ ;

// No general `;`-starts-a-comment rule (unlike grammar/RccAsm.g4) -
// IRPrinter.cpp never writes a trailing comment after real content,
// only the two fixed whole-line headers above, so every other `;` in
// the input is a real SEMI.
WS : [ \t\r\n]+ -> skip ;

// `asm { ... }` - see IRPrinter.cpp's StmtKind::Asm: raw text,
// captured verbatim exactly like CLexer.g4's own ASM_MODE (no nested
// braces, same assumption carried over from the source language
// itself - see that grammar's comment).
ASM_BEGIN : 'asm' [ \t\r\n]* '{' -> pushMode(ASM_MODE) ;

mode ASM_MODE;
ASM_TEXT : ~[}]+ ;
ASM_END  : '}' -> popMode ;
