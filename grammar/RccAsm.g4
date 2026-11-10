grammar RccAsm;

// ---------------------------------------------------------------------
// One shared textual "assembly IR" grammar used by every rcc backend's
// own in-process assembler (pdp7/MiniAssembler.cpp today; the 8086
// backend's own assembler is meant to reuse it too, once built - see
// ../docs/language.md). Each backend emits its own dialect of
// *this* syntax (different mnemonics, different operand conventions)
// and each backend's own semantic layer interprets the parsed operand
// list according to its own target's real semantics:
//
//  - pdp7: every syllable of every operand, plus the mnemonic token
//    itself, OR-combine (or '+'/'-' combine, if signed) into one
//    18-bit expression value - matching real PDP-7 assembly, where an
//    "opcode" is just a symbol whose value happens to be an
//    instruction's bit pattern, indistinguishable at the syntax level
//    from any other symbol (see pdp7/MiniAssembler.cpp's own comment
//    for the full story, including why this grammar has no separate
//    "instruction" concept the way a conventional assembler would).
//  - 8086: the mnemonic selects a real x86 instruction and each
//    comma-separated operand is a separate, structurally distinct
//    argument to it - ordinary NASM semantics.
//
// Deliberately NOT a general-purpose assembly grammar: no macros, no
// section directives, nothing beyond what rcc's own backends are ever
// observed to emit. In particular this is NOT
// ../../pdp-assembler/grammar/Pdp7.g4 (the *historical* as.s/as7
// grammar, handling real kernel sources with packed-char literals,
// numeric relative labels, `@local` directives, and `"`-comments) -
// this one is rcc's own, much narrower, and uses `;` for comments
// (matching 8086/NASM's own convention, rather than inventing a third
// one) on both targets.
// ---------------------------------------------------------------------

program
    : line* EOF
    ;

// Always exactly one statement per physical line - no `;`-joined
// multi-statement lines the way the historical as.s/as7 grammar
// allows (`;` is this grammar's comment character instead - see
// COMMENT below). Every rcc backend emits one label-and/or-statement
// per line, so this grammar doesn't need that extra complexity; a
// multi-word data declaration is just several consecutive lines
// instead (see pdp7/Pdp7Backend.cpp's `declareArray`).
line
    : label* statement? NEWLINE
    ;

label
    : IDENT COLON
    ;

statement
    : lhs=IDENT EQUALS rhs=expr                                                      # assignStmt
    // NASM's "times COUNT instruction" repeat-prefix (8086's boot-
    // sector/placement padding, `times (510-($-$$)) db 0`) - the
    // only construct here that's structurally a prefix on another
    // statement rather than a flat mnemonic + operands. `count`
    // needs its own parens (not just whatever grouping its own
    // expression happens to use internally, e.g. the `($-$$)` above)
    // because `expr` is otherwise unbounded - with no required
    // separator between `count` and `mnemonic`, a bare trailing
    // identifier (an unsigned syllable, which `expr` allows any
    // number of) is syntactically indistinguishable from the
    // following mnemonic, and a parser has no semantic reason to
    // prefer one reading over the other. The closing ')' here gives
    // it one.
    | 'times' LPAREN count=expr RPAREN mnemonic=(IDENT | NUMBER) (operand (COMMA operand)*)?  # timesStmt
    | mnemonic=(IDENT | NUMBER) (operand (COMMA operand)*)?                           # instrStmt
    ;

// `size` is an optional leading bare word ("byte"/"word", whatever a
// given backend's own semantic layer chooses to recognize) right
// before a bracketed memory operand - e.g. x86's `mov byte [ebp-4],al`
// - distinguishing how many bytes of memory that operand addresses
// from the register operand's own width. Every backend that never
// emits one (every backend before I386Codegen.cpp's own narrower-
// than-native-int loads/stores) simply never has `size` set; this is
// purely additive syntax.
operand
    : size=IDENT? LBRACKET expr RBRACKET
    | expr
    ;

// One or more signed syllables. pdp7's own semantics OR-combine an
// unsigned (no leading '+'/'-') syllable with whatever came before it
// within the same operand (e.g. "jmp i F3" is a single operand made
// of two syllables, "i" and "F3", both unsigned - OR'd together,
// exactly like real pdp7as expression syntax); 8086 only ever uses
// multi-syllable expr for '+'/'-' arithmetic (`label+bx`,
// `510-($-$$)`) and otherwise sticks to single-syllable operands - it
// never relies on the "unsigned means OR" default at all. Each
// backend's own expression evaluator decides which convention
// applies; the grammar only captures the shared *shape*.
expr
    : syllable syllable*
    ;

syllable
    : sign? atom
    ;

sign
    : PLUS
    | MINUS
    ;

atom
    : IDENT
    | NUMBER
    | LPAREN expr RPAREN
    ;

IDENT   : [A-Za-z_.$][A-Za-z0-9_.$]* ;
// Decimal, pdp7-style octal (leading '0', e.g. its `ion`/`iof` literal
// instruction words), or 8086-style hex (`0x7C00`).
NUMBER  : '0x' [0-9a-fA-F]+ | [0-9]+ ;

COLON    : ':' ;
EQUALS   : '=' ;
COMMA    : ',' ;
PLUS     : '+' ;
MINUS    : '-' ;
LBRACKET : '[' ;
RBRACKET : ']' ;
LPAREN   : '(' ;
RPAREN   : ')' ;

COMMENT : ';' ~[\r\n]* -> skip ;
NEWLINE : '\r'? '\n' ;
WS      : [ \t]+ -> skip ;
