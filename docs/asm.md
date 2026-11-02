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
and what `--dump-asm` additionally saves to `build/<name><ext>` - see
`docs/using.md`'s own `--dump-asm` row for every target's own
extension.

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
*means* is entirely up to each backend's own semantic layer. Every
target in `docs/platforms.md` shares this exact grammar and parses
against it with its own in-process assembler
(`src/codegen/<target>/<Target>Assembler.cpp`) - what differs is only
how each one interprets a parsed mnemonic/operand list. Two genuinely
different interpretation styles exist, and every target follows one
or the other:

- **pdp7** — the one real outlier: every syllable of every operand,
  plus the mnemonic token itself, OR-combine (or `+`/`-` combine, if
  signed) into one 18-bit expression value, matching real PDP-7
  assembly: an "opcode" there is just a symbol whose value happens to
  be an instruction's bit pattern, indistinguishable at the syntax
  level from any other symbol. An unsigned (no leading `+`/`-`)
  syllable ORs with whatever came before it in the same operand (e.g.
  `jmp i F3` is one operand made of two syllables, `i` and `F3`, OR'd
  together).
- **every other target** — the mnemonic selects a real target
  instruction and each comma-separated operand is a separate,
  structurally distinct argument to it, the same "ordinary NASM
  semantics" **8086**/**8088** established first: multi-syllable
  `expr` is only ever used for `+`/`-` arithmetic (`label+bx`,
  `510-($-$$)`), never pdp7's "unsigned means OR" default. See each
  target's own `<Target>Codegen.h` header comment (linked from its
  own entry in `docs/platforms.md`) for the real mnemonics/operand
  conventions it actually emits - register model, branch-range or
  immediate-width workarounds, Harvard code/data split, and so on all
  vary by target, but the grammar-level interpretation style doesn't.

This grammar is deliberately not
`../../pdp-assembler/grammar/Pdp7.g4` (the historical `as.s`/`as7`
grammar, which handles real kernel sources with packed-char literals,
numeric relative labels, `@local` directives, and `"`-comments) — this
one is pcc's own, much narrower, and uses `;` for comments on every
target rather than inventing a third convention.

## Where it's used

- `grammar/PccAsm.g4` is the single source grammar (lexer + parser
  combined); generated code lives in `src/generated/` and is checked
  into git (see `docs/building.md` for the regeneration workflow).
- Every backend's own `src/codegen/<target>/<Target>Assembler.cpp`
  (e.g. `src/codegen/pdp7/Pdp7Assembler.cpp`,
  `src/codegen/x86_8086/I8086Assembler.cpp`,
  `src/codegen/sparc/SparcAssembler.cpp`) parses against this grammar
  and then interprets the result per its own target semantics, as
  above - see `docs/platforms.md` for the full target list.
