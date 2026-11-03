# The IR format

Between the C frontend and every backend sits one target-independent
intermediate representation (`src/ir/IR.h`). It's deliberately simple:
every variable, temporary and literal is a single statically allocated
slot — no stack, no recursion — so the same IR works for every target
pcc supports.

`src/ir/IRPrinter.{h,cpp}` renders a `Program` as human-readable
pseudo-code. That text is what `--dump-ir` writes to `build/<name>.ir`,
and it's also a real, independently parseable language: every compile
internally re-derives it regardless of `--dump-ir`, and
`src/ir/IRValidator.h` parses it against the grammar below as a
standing self-check that the printer and the grammar describing it
haven't drifted apart — a mismatch throws as an internal pcc bug,
never a user source error.

## Example

```
; --- globals ---
int count#1 = 0;
int buf#2[16];

; --- functions ---
int main#3()
{
  local x#4 = (1 + 2);
  if ((x#4 > 0))
  {
    return x#4;
  }
  return 0;
}
```

## Shape

Described by `grammar/PccIRLexer.g4` + `grammar/PccIRParser.g4`:

```
program    : '; --- globals ---' globalDecl*
             '; --- functions ---' funcDecl* EOF
globalDecl : 'int' ref ('[' NUMBER ']' ('=' initList)? | ('=' NUMBER)?) ';'
funcDecl   : 'int' ref '(' (param (',' param)*)? ')' block
param      : 'int' ref
block      : '{' stmt* '}'

stmt : block
     | 'if' '(' expr ')' stmt ('else' stmt)?
     | 'while' '(' expr ')' stmt
     | 'return' expr? ';'
     | 'local' ref ('[' NUMBER ']' ('=' initList)? | ('=' expr)?) ';'
     | 'asm' '{' <raw target text> '}'
     | ';'
     | expr ';'

ref  : IDENT '#' NUMBER        // "name#id" - a Slot or Function reference
expr : NUMBER                                    // int literal
     | ref '[' expr ']' ('=' expr)?              // index, or indexed assign
     | ref '=' expr                              // assign
     | '*' '(' expr ')' ('=' expr)?               // deref read, or deref assign
     | ('-'|'!'|'~'|'&') '(' expr ')'              // unary
     | '(' expr binOp expr ')'                    // binary
     | ref '(' (expr (',' expr)*)? ')'            // call
     | '(' expr ')' '(' ')'                       // call through a computed address - always 0 args
     | IDENT '(' ')'                               // intrinsic, e.g. _ion()
     | ref                                        // variable reference
```

- `ref` (`name#id`) names a `Slot` or `Function` — the `#id` suffix
  disambiguates locals that shadow across scopes or share a name with
  a global; every reference to the same slot prints the same id.
- Every binary and unary expression is always fully parenthesized by
  the printer, so the grammar needs no operator-precedence table at
  all — it only has to accept the exact shapes `IRPrinter.cpp` ever
  writes.
- A local array's initializer is always an `initList` (`= {1, 2, 3}`),
  never a general `expr` the way a scalar local's can be — exactly
  the same literal-or-symbol list a global array carries (see `IR.h`'s
  `ArrayInitElem`), since every local is laid out statically too (see
  this file's own "The IR is AST.h's tree shape..." header comment).
- `asm { ... }` is a `Stmt::Asm` node: raw target-specific assembly
  text, carried through the IR verbatim and printed back out
  unparsed — this is the only place a backend's own assembly syntax
  (see `docs/asm.md`) appears inside IR text.

## Where it's used

- `src/ir/IR.h` defines the real in-memory `Program`/`Slot`/
  `Function`/`Stmt`/`Expr` types that every backend's codegen
  actually walks; this document describes `IRPrinter.cpp`'s *text*
  rendering of that structure, not the C++ types themselves.
- `src/ir/IRBuilder.h` builds a `Program` from the parsed C AST.
- `src/ir/IRPrinter.h` renders it to this text (`--dump-ir`,
  `IRValidator.h`'s self-check).
- `src/optimizers/` (see `docs/optimization.md`) runs IR-to-IR passes
  between `IRBuilder` and codegen — one optimizer written once
  benefits every backend, since every target-specific decision
  happens strictly after this IR exists.
- Every backend's codegen (`src/codegen/<target>/`) consumes a
  `Program` directly and lowers it to that target's own assembly
  dialect (`docs/asm.md`).
