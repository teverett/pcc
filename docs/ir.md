# The IR format

Between the C frontend and every backend sits one target-independent
intermediate representation (`src/ir/IR.h`). It's deliberately simple:
a Global is one statically allocated slot, while a Local or Param
lives in a per-call frame (`Slot::frameOffset`/`Function::frameSize`,
assigned once by `IRBuilder`, in declaration order) that every
backend gives its own fresh storage on every call - so direct and
indirect/mutual recursion compute correctly, bounded only by whatever
stack (hardware or software) that backend builds its frames on. The
same IR works for every target pcc supports.

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
  local int x#4 = (1 + 2);
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

// The canonical, shortest real-C spelling of a pcc::Type (src/Type.h)
// - IRPrinter.cpp's typeSpecText(): always this one fixed spelling per
// (base, signedness) pair, never the source grammar's fuller
// combinatorial typeSpec (e.g. "short int"/"signed short").
typeSpec   : 'unsigned'? 'char'
           | 'unsigned'? 'short'
           | 'unsigned'? 'int'
           | 'unsigned'? 'long' 'long'
           | 'unsigned'? 'long'

globalDecl : typeSpec ref ('[' NUMBER ']' ('=' initList)? | ('=' NUMBER)?) ';'
funcDecl   : typeSpec ref '(' (param (',' param)*)? ')' block
param      : typeSpec ref
block      : '{' stmt* '}'

stmt : block
     | 'if' '(' expr ')' stmt ('else' stmt)?
     | 'while' '(' expr ')' stmt ('post' stmt)?
     | 'return' expr? ';'
     | 'break' ';'
     | 'continue' ';'
     | 'local' typeSpec ref ('[' NUMBER ']' ('=' initList)? | ('=' expr)?) ';'
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
     | '(' typeSpec ')' '(' expr ')'              // Convert - an implicit or explicit type conversion
     | ref '(' (expr (',' expr)*)? ')'            // call
     | '(' expr ')' '(' ')'                       // call through a computed address - always 0 args
     | IDENT '(' ')'                               // intrinsic, e.g. _ion()
     | ref                                        // variable reference
```

- `ref` (`name#id`) names a `Slot` or `Function` — the `#id` suffix
  disambiguates locals that shadow across scopes or share a name with
  a global; every reference to the same slot prints the same id.
- `Convert` is every implicit conversion the type-checking pass folded
  into `IRBuilder::buildExpr` (integer promotion, the usual arithmetic
  conversions, assignment/return/call-argument conversion) *and* a
  source-level cast `(type)expr` — both print and parse identically,
  since a cast is exactly this with no separate node of its own; see
  `IR.h`'s `ExprKind::Convert`.
- Every binary and unary expression is always fully parenthesized by
  the printer, so the grammar needs no operator-precedence table at
  all — it only has to accept the exact shapes `IRPrinter.cpp` ever
  writes.
- A local array's initializer is always an `initList` (`= {1, 2, 3}`),
  never a general `expr` the way a scalar local's can be — exactly
  the same literal-or-symbol list a global array carries (see `IR.h`'s
  `ArrayInitElem`); a backend re-applies that same list to the array's
  own per-call frame storage on every activation, since (unlike a
  global) it's never a single static cell.
- `while`'s own optional `post` clause is a desugared `for` loop's
  post-statement (`for (init; cond; post) body` becomes `{ init;
  while (cond) body` with `post` printed as that `while`'s own
  trailing `post` clause, not folded into the loop body - see `IR.h`'s
  `Stmt::postStmt`) - `continue` needs to run it before the condition
  is retested, even though it skips the rest of the loop body.
- `asm { ... }` is a `Stmt::Asm` node: raw target-specific assembly
  text, carried through the IR verbatim and printed back out
  unparsed — this is the only place a backend's own assembly syntax
  (see `docs/asm.md`) appears inside IR text.

## Fixed absolute placement

A function or global declared with a trailing `@ addr` (e.g.
`int vector() @ 020 { ... }`) carries `hasAddress=true`/`address=addr`
on its `IR.h` `Function`/`Slot` (the grammar's own `'@' addr=NUMBER`,
on `functionDecl`/`globalDecl` — see `grammar/CParser.g4`). This pins
that function's entry point, or that global's storage, to an exact
spot instead of wherever the backend would otherwise lay it out —
meant for things a real machine's own hardware fixes in place
regardless of what the compiler would pick, like a PDP-7 kernel's
interrupt/trap vector cells.

`addr` is always an *offset from that backend's own relocation base*,
never a true absolute address counted from machine address 0 — each
backend's own codegen comment at its placement call site documents
exactly what its own base is (e.g. pdp7's is `pdp7as`'s own `..`
symbol, matching the real kernel's historical `. = orig+N`
convention). A placed function/global is still emitted in the
program's normal declaration order; placing it only changes where its
storage/entry point ends up, wrapped in a save/jump-to-address/restore
of the assembler's own location counter around that one item, so
whatever comes right after it in program order resumes exactly where
it would have without the placement at all.

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
