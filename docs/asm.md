# The assembly format

pcc's backends share one textual assembly grammar,
`grammar/PccAsm.g4` — not a general-purpose assembly syntax, but
exactly the shape every pcc backend's own in-process assembler ever
needs to parse: no macros, no section directives, nothing beyond what
pcc's own codegen is ever observed to emit. Each backend emits its own
dialect of this shared syntax (different mnemonics, different operand
conventions) and interprets the parsed operand list according to its
own target's real semantics.

This is what `--format=asm` (compile mode's default output) writes,
and what `--dump-asm` additionally saves to `build/<name><ext>`
(`.s` for pdp7, `.asm` for 8086/8088).

## Shape

```
line       : label* statement? NEWLINE
label      : IDENT ':'
statement  : IDENT '=' expr                                   // assignStmt
           | 'times' '(' expr ')' (IDENT | NUMBER) operand*   // timesStmt
           | (IDENT | NUMBER) operand*                        // instrStmt
operand    : '[' expr ']' | expr
expr       : syllable syllable*
syllable   : ('+' | '-')? atom
atom       : IDENT | NUMBER | '(' expr ')'
```

- Exactly one statement per physical line — a multi-word data
  declaration is just several consecutive lines, never a `;`-joined
  multi-statement line.
- `;` starts a comment (matching NASM's convention, used on both
  targets), runs to end of line, and is otherwise never special.
- `NUMBER` is decimal, pdp7-style octal (a leading `0`, e.g. its
  `ion`/`iof` literal instruction words), or 8086-style hex (`0x7C00`).
- `times (COUNT) mnemonic operand, ...` is NASM's repeat-prefix
  (used for the 8086 boot-sector's placement padding, e.g.
  `times (510-($-$$)) db 0`). It needs its own parens around `count`
  because `expr` is otherwise unbounded: with no required separator
  between `count` and `mnemonic`, a bare trailing identifier is
  syntactically indistinguishable from the following mnemonic, and
  the closing `)` is what tells the parser where `count` ends.

## Per-target semantics

The grammar only captures the shared shape; what a parsed statement
*means* is entirely up to each backend's own semantic layer:

- **pdp7** — every syllable of every operand, plus the mnemonic token
  itself, OR-combine (or `+`/`-` combine, if signed) into one 18-bit
  expression value, matching real PDP-7 assembly: an "opcode" there is
  just a symbol whose value happens to be an instruction's bit
  pattern, indistinguishable at the syntax level from any other
  symbol. An unsigned (no leading `+`/`-`) syllable ORs with whatever
  came before it in the same operand (e.g. `jmp i F3` is one operand
  made of two syllables, `i` and `F3`, OR'd together).
- **8086/8088** — the mnemonic selects a real x86 instruction and each
  comma-separated operand is a separate, structurally distinct
  argument to it: ordinary NASM semantics. Multi-syllable `expr` is
  only ever used for `+`/`-` arithmetic (`label+bx`, `510-($-$$)`); it
  never relies on pdp7's "unsigned means OR" default.
- **6502** — same shared grammar, parsed by its own in-process
  assembler (`src/codegen/m6502/M6502Assembler.cpp`); see
  `M6502Codegen.h`'s own header comment for the mnemonics and operand
  conventions `M6502Codegen` emits (branch-range workarounds, the
  three-register model, etc.).
- **65c02** — the same dialect as `6502` above, plus exactly one
  extra mnemonic pair (`stz`/`stzp`, for the CMOS-only STZ
  instruction), parsed by its own in-process assembler
  (`src/codegen/m65c02/M65C02Assembler.cpp`); see `M65C02Codegen.h`'s
  own header comment.

This grammar is deliberately not
`../../pdp-assembler/grammar/Pdp7.g4` (the historical `as.s`/`as7`
grammar, which handles real kernel sources with packed-char literals,
numeric relative labels, `@local` directives, and `"`-comments) — this
one is pcc's own, much narrower, and uses `;` for comments on both
targets rather than inventing a third convention.

## Where it's used

- `grammar/PccAsm.g4` is the single source grammar (lexer + parser
  combined); generated code lives in `src/generated/` and is checked
  into git (see the top-level README's "Building" section for the
  regeneration workflow).
- `src/codegen/pdp7/Pdp7Assembler.cpp`,
  `src/codegen/x86_8086/I8086Assembler.cpp`,
  `src/codegen/m6502/M6502Assembler.cpp`, and
  `src/codegen/m65c02/M65C02Assembler.cpp` each parse against this
  grammar and then interpret the result per their own target
  semantics, as above.
