lexer grammar CLexer;

// ---------------------------------------------------------------------
// Lexer half of rcc's C-subset grammar (see CParser.g4 for
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
CHAR_KW  : 'char' ;
SHORT_KW : 'short' ;
LONG_KW  : 'long' ;
SIGNED_KW: 'signed' ;
UNSIGNED_KW: 'unsigned' ;
VOID_KW  : 'void' ;
SIZEOF_KW: 'sizeof' ;
IF_KW    : 'if' ;
ELSE_KW  : 'else' ;
WHILE_KW : 'while' ;
FOR_KW   : 'for' ;
SWITCH_KW: 'switch' ;
CASE_KW  : 'case' ;
DEFAULT_KW: 'default' ;
RETURN_KW: 'return' ;
BREAK_KW : 'break' ;
CONTINUE_KW: 'continue' ;
GOTO_KW  : 'goto' ;
EXTERN_KW: 'extern' ;
STATIC_KW: 'static' ;
STRUCT_KW: 'struct' ;
ENUM_KW  : 'enum' ;
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
// Compound-assignment operators, '++'/'--', and '?'/':' (the ternary
// operator) - each lexed as its own maximal-munch token ('+=' always
// wins over 'PLUS' then 'ASSIGN' separately, same automatic longest-
// match rule that already makes '<=' beat '<' above) ahead of
// ASSIGN/PLUS/etc. being tried alone.
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
INC       : '++' ;
DEC       : '--' ;
QUESTION  : '?' ;
COLON     : ':' ;
DOT       : '.' ;
ARROW     : '->' ;

IDENT   : [a-zA-Z_][a-zA-Z_0-9]* ;
// Decimal, octal (leading 0), or hex (0x/0X), same spelling as ISO C's
// integer-constant rule, plus an optional u/l suffix that picks the
// literal's type (unsigned/long/long long, same spelling as ISO C's
// integer-suffix rule) - BuildAst parses and validates the digits and
// the suffix text itself; this rule only admits the syntax, same as
// every other token here that defers meaning to BuildAst.
NUMBER  : ( '0' [xX] [0-9a-fA-F]+ | '0' [0-7]* | [1-9] [0-9]* ) [uUlL]* ;
// A single character, no escapes - enough for ASCII-arithmetic use
// like '0' + digit; see ../README.md's "Scope and limitations".
CHARLIT : '\'' ~['\r\n] '\'' ;
// A double-quoted byte string: \\ \" \n \t \r, an octal escape (\ooo, 1-3
// octal digits, same spelling as ISO C's), or a hex escape (\xhh..., one or
// more hex digits, same spelling as ISO C's) - BuildAst decodes
// STRINGLIT_ESC itself (including the octal/hex value -> byte conversion);
// this rule only admits the syntax. Still no line continuation - a string
// literal can't span a physical line (see docs/language.md's "Scope and
// limitations").
STRINGLIT : '"' (STRINGLIT_ESC | ~["\\\r\n])* '"' ;
fragment STRINGLIT_ESC : '\\' ( [0-7] [0-7]? [0-7]? | 'x' [0-9a-fA-F]+ | ["\\ntr] ) ;

LINE_COMMENT  : '//' ~[\r\n]* -> skip ;
BLOCK_COMMENT : '/*' .*? '*/' -> skip ;
WS            : [ \t\r\n]+ -> skip ;

// `asm { ... }` is an escape hatch, not a parsed sub-language: the
// raw text between the braces (ASM_TEXT, captured verbatim here - no
// nested braces, since no target's own assembly dialect uses them)
// is emitted into the generated assembly exactly as written, with no
// attempt by rcc to understand it. Meant for real hardware
// instructions this C subset has no portable expression for at all
// (e.g. the PDP-7 disk controller's raw IOT instructions -
// `dscs`/`dslw`/.../`dsrs`) - see ../docs/language.md's "Inline
// asm" for the ground rules (target-specific by construction, no
// return value, caller's own job to keep AC/registers in a sane
// state around it).
ASM_BEGIN : 'asm' [ \t\r\n]* '{' -> pushMode(ASM_MODE) ;

mode ASM_MODE;
ASM_TEXT : ~[}]+ ;
ASM_END  : '}' -> popMode ;
