lexer grammar CLexer;

// ---------------------------------------------------------------------
// Lexer half of pdp-c-compiler's C-subset grammar (see CParser.g4 for
// the parser half and ../README.md for what the language actually
// is). Split from a single combined grammar specifically so `asm
// { ... }` (below) can use a real lexer mode to capture its raw body
// verbatim - ANTLR4 lexical modes aren't available in a combined
// parser+lexer grammar, only in a lexer-only one like this.
// ---------------------------------------------------------------------

// Keyword/punctuation tokens CParser.g4 uses as bare string literals.
// A split grammar (unlike a combined one) can't auto-create these
// from the parser's own literals - each needs an explicit rule here.
INT_KW   : 'int' ;
IF_KW    : 'if' ;
ELSE_KW  : 'else' ;
WHILE_KW : 'while' ;
FOR_KW   : 'for' ;
RETURN_KW: 'return' ;
BREAK_KW : 'break' ;
CONTINUE_KW: 'continue' ;
LPAREN   : '(' ;
RPAREN   : ')' ;
LBRACK   : '[' ;
RBRACK   : ']' ;
LBRACE   : '{' ;
RBRACE   : '}' ;
AT       : '@' ;
SEMI     : ';' ;
COMMA    : ',' ;
ASSIGN   : '=' ;
STAR     : '*' ;
AMP      : '&' ;
MINUS    : '-' ;
BANG     : '!' ;
TILDE    : '~' ;
SLASH    : '/' ;
PERCENT  : '%' ;
PLUS     : '+' ;
SHL      : '<<' ;
SHR      : '>>' ;
LE       : '<=' ;
GE       : '>=' ;
LT       : '<' ;
GT       : '>' ;
EQ       : '==' ;
NE       : '!=' ;
CARET    : '^' ;
PIPE     : '|' ;
ANDAND   : '&&' ;
OROR     : '||' ;

IDENT   : [a-zA-Z_][a-zA-Z_0-9]* ;
NUMBER  : [0-9]+ ;
// A single character, no escapes - enough for ASCII-arithmetic use
// like '0' + digit; see ../README.md's "Scope and limitations".
CHARLIT : '\'' ~['\r\n] '\'' ;

LINE_COMMENT  : '//' ~[\r\n]* -> skip ;
BLOCK_COMMENT : '/*' .*? '*/' -> skip ;
WS            : [ \t\r\n]+ -> skip ;

// `asm { ... }` is an escape hatch, not a parsed sub-language: the
// raw text between the braces (ASM_TEXT, captured verbatim here - no
// nested braces, since no target's own assembly dialect uses them)
// is emitted into the generated assembly exactly as written, with no
// attempt by pcc to understand it. Meant for real hardware
// instructions this C subset has no portable expression for at all
// (e.g. the PDP-7 disk controller's raw IOT instructions -
// `dscs`/`dslw`/.../`dsrs`) - see ../docs/pdp-c-compiler.md's "Inline
// asm" for the ground rules (target-specific by construction, no
// return value, caller's own job to keep AC/registers in a sane
// state around it).
ASM_BEGIN : 'asm' [ \t\r\n]* '{' -> pushMode(ASM_MODE) ;

mode ASM_MODE;
ASM_TEXT : ~[}]+ ;
ASM_END  : '}' -> popMode ;
