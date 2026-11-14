# The rcc language and object model

This is the language-level reference for rcc's own C subset, plus the
handful of project-specific choices (its own COFF variant, its
`asm { ... }` escape hatch, its text preprocessor) that don't belong in
`docs/building.md`/`docs/using.md` (the CLI) or `docs/platforms.md`
(what each backend's own hardware is).

## The language subset

Types: `char`, `short`, `int`, `long`, `long long`, each `signed`
(the default) or `unsigned`; a pointer to any of those, at any depth
(`T*`, `T**`, ...); a fixed-size array of any of those or of a
pointer. A function always declares an explicit return type and may be
declared with a trailing `;` instead of a body as a prototype,
resolved by name against whichever file actually defines it regardless
of textual order. `void` is a valid return type (a function that
returns no value, via a bare `return;` or by falling off its closing
`}`); it isn't usable anywhere else `void*`, a `void` parameter, a
`void` variable, etc. - see "Scope and limitations" below.

Expressions: the usual arithmetic/comparison/logical/bitwise operators
(see "Bitwise operators" below for that group specifically), `=` and
the ten compound-assignment operators (`+= -= *= /= %= &= ^= |= <<=
>>=`), prefix/postfix `++`/`--`, the ternary operator (`cond ? then :
els`), `&`/`*` (address-of/dereference), `arr[i]` indexing, `f(...)`
calls (including a call through a computed address, `(expr)()` — see
`IR.h`'s `ExprKind::CallPtr`), an explicit `(type)expr` cast,
`sizeof(type)`/`sizeof(expr)`, and the three literal forms: a `NUMBER`
(with an optional `u`/`U`/`l`/`L`/`ll`/`LL` integer suffix picking its
type, same spelling as ISO C), a `CHARLIT` (`'x'`), and a `STRINGLIT`
(`"..."`) — see "Scope and limitations" below for what each literal
form does and doesn't support.

Statements: `{ }` blocks, `if`/`else`, `while`, `for` (desugared into
a block containing a `while`, with the post-statement kept separate so
`continue` still runs it — see `IR.h`'s own `Stmt::postStmt`),
`return`, `break`, `continue`, `switch`/`case`/`default` (real C
fallthrough; desugared entirely in `IRBuilder.cpp` into a `Block` of
comparisons dispatching into a sequence of `StmtKind::Label`-marked
case bodies joined by `StmtKind::Goto` — see `IR.h`'s own comment on
why those two exist at the IR level, and IRBuilder's `buildSwitch`),
`goto`/labels (`name: stmt` attaches to exactly that one statement,
same as real C — a label opens no scope of its own, so a declaration
right after one stays visible to whatever follows in the enclosing
block; labels have function scope, resolved against a table collected
before the function body is built, so a forward `goto` works — see
IRBuilder's `collectLabels`), a local declaration (scalar or array,
optionally initialized), a bare expression statement, and
`asm { ... }` (see "Inline asm" below).

Not in this subset: `union`/`typedef`, floating point, variadic
functions, and designated initializers. `struct` is supported, in the
narrower form described in "Structs" below; `enum` is supported too,
in the narrower form described in "Enums" below.

## Scope and limitations

Deliberate simplifications, not bugs:

- `void` is only a function's return type; it's not a `baseTypeSpec`
  alternative, so it never appears as a variable/parameter type, a
  cast/`sizeof` operand, or `void*` - none of those parse. A `void`
  function's `return;` must have no value (`return expr;` is a
  compile error), and calling one is only valid as a bare expression
  statement - using its (nonexistent) result as a value anywhere else
  (an operand, an argument, assigned, `sizeof`'d, ...) is a compile
  error too.
- Integer promotion always promotes a sub-`int`-rank operand to
  **signed** `int`, even in the rare case where real C would promote
  `unsigned short` to `unsigned int` because `short`/`int` happen to
  be the same width on a given target.
- The "usual arithmetic conversions" pick the operand of strictly
  greater rank (`char` < `short` < `int` < `long` < `long long`, the
  fixed order ISO C itself specifies) independent of a target's actual
  per-type widths; on equal rank, unsigned wins. The classic edge case
  where a same-width higher-rank signed type can't represent a
  lower-rank unsigned type's full range (forcing both to an unsigned
  version of the higher-rank type) isn't implemented — this keeps
  promotion entirely target-independent, with no width table needed to
  rank two types against each other (see `src/Type.h`/`src/Type.cpp`).
- An unsuffixed numeric literal is always `int`; it does not
  auto-widen to `long`/`long long` on overflow — use a suffix (`10L`,
  `4000000000UL`) for anything that needs to be wider.
- A `NUMBER` follows ISO C's integer-constant spelling: decimal
  (`[1-9][0-9]*`), a leading-zero octal constant (`0[0-7]*`), or a
  `0x`/`0X` hex constant, each with the usual integer suffix
  (`CLexer.g4`'s own token). `0` itself parses as decimal zero, not as
  an octal prefix with no digits.
- A real pointer (`T*`) has scaled arithmetic (`p + 1` moves by
  `sizeof(T)`) and a typed dereference (`*p` reads/writes exactly
  `sizeof(T)`, correctly sign/zero-extended); a plain `int` used as an
  untyped address (the language's original, still-fully-supported
  style, from before `T*` existed) keeps unscaled, native-word-
  granular arithmetic and a full-native-word dereference regardless of
  what it conceptually points to. Pick a real `T*` for anything that
  needs correct, portable scaling.
- An array never implicitly decays to a pointer — indexing (`arr[i]`)
  or an explicit `&arr[0]`/`&arr` is required to get its address; a
  bare array name used anywhere else is a compile error.
- An array's initializer is always a literal list (`{1, -2, 3}`; each
  literal may be negated), or a bare symbol name standing for that
  symbol's own address — the
  dispatch-table idiom a jump table needs (`int t[] = {f, g};`) — never
  a general expression. Under real separate compilation, a bare name
  here (or after `&`) that isn't defined, called, or declared `extern`
  somewhere earlier in *this* file is a compile error, not a silent
  guess — see "Extern declarations" below for why and what to add.
- `CHARLIT` (`'x'`) has no escape sequences at all: exactly one raw
  character between the quotes (enough for ASCII-arithmetic use like
  `'0' + digit`).
- `STRINGLIT` (`"..."`) supports `\\ \" \n \t \r`, an octal escape
  (`\ooo`, 1-3 octal digits, e.g. `\101` is `'A'`), and a hex escape
  (`\xhh...`, one or more hex digits), all with the same spelling as
  ISO C's; an escape value wider than a byte truncates to its low 8
  bits, also matching ISO C's own overflow behavior. Still no line
  continuation (a string literal can't span a physical line) — unlike
  the escape set above, that's not a per-string-literal feature in
  real C either (it's a universal backslash-newline splice applied to
  the whole source before tokenizing, phase 2 of translation), and
  this project doesn't support it anywhere, including inside a
  multi-line `#define` (see `src/preprocessor/Preprocessor.cpp`'s own
  header comment). It lowers to the address of an
  anonymous, NUL-terminated `char` array holding its decoded bytes —
  exactly what `&someCharArray` already means for a hand-declared one
  (see `IRBuilder.cpp`'s own `buildExpr` case for `ExprKind::
  StringLit`), so no backend needs its own string-literal codegen at
  all.

## Structs

`struct Tag { member; ... };` at file scope only (no nested, local, or
anonymous struct definitions, and no forward declaration — a tag must
be fully defined, textually, before any use of it). A member is any
type this subset otherwise supports (including another already-defined
struct, or a fixed-size array of one), laid out in declaration order
with each member aligned to its own width and the struct's own overall
size padded up to its widest member's alignment (so an array of a
struct, or a struct nested inside another, always starts aligned).
`sizeof(struct Tag)` and `struct Tag *` (any depth) both work the same
as for any other type.

Member access is `expr.name` or `expr->name` (indexing straight into
an array member in the same step, e.g. `buf.data[2]`, is allowed too),
chainable (`a.b.c`), and works as both a read and an assignment target
(`=` and the compound-assignment operators). `.` requires a struct
value on its left (a variable, an array element, a dereferenced
pointer, or another member); `->` requires a pointer to one.

An optional `@packed` after the tag (`struct Tag @packed { ... };`)
disables inter-member padding: every member is laid out back-to-back
with no gaps, the struct's own alignment becomes 1, and its overall
size is the exact byte sum with no trailing padding either (see
`src/StructLayout.h`/`src/StructLayout.cpp`). See
[`docs/known-bugs.md`](known-bugs.md) for a target-specific case where
this can still misalign a packed struct in memory.

Not supported: passing or returning a struct by value (only a pointer
to one, as a parameter or return type), whole-struct assignment
(`a = b` where both are struct-typed), a struct initializer (`= {...}`
on a struct-typed declaration, including an array of structs — a
struct variable starts uninitialized, like any other local with no
initializer, or implicitly zero-filled as a global, like any other
global), and an array of structs at file scope (a local array of
structs works; see `IRBuilder.cpp`'s own comment on why only the global
case is excluded). Each of these is a compile error, not a silent
miscompile.

## Enums

`enum Tag { Member, ... };` at file scope only, same as `struct` — no
forward declaration (a tag must be fully defined, textually, before
any use of it or of its members). Unlike `struct`, an enum has no
runtime representation of its own at all: `enum Tag` is just another
spelling for `int` once resolved, and every member is a plain `int`
compile-time constant, substituted directly wherever it's used —
nothing downstream of parsing (the IR, any backend) ever sees `enum`
in any form. `sizeof(enum Tag)` is therefore always exactly
`sizeof(int)`.

A member with no `= value` takes the previous member's value plus one
(`0` for the first member); a member that gives one may reuse an
already-assigned value, same as real C, but may not reuse an
already-declared member's own *name* — nor may a tag be reused. A
member's value must be a plain (optionally negated) integer literal,
not a general expression (the same restriction a `struct` member's
array size already has).

An enum constant is usable anywhere a `NUMBER` literal already was: as
a general expression, a `switch`'s `case` label, and a global's or
`static` local's initializer. It is *not* usable as an array size
(that grammar slot only ever takes a literal `NUMBER`) or as an array
initializer element: that slot also accepts a bare identifier, but
only as a function/global's own address (the dispatch-table idiom —
see "Scope and limitations" above), never as an enum constant's value.

An enum constant shares no namespace-collision checking with ordinary
variables/parameters/functions (this subset's symbol tables for those
are built later, in `IRBuilder`, after enum constants are already
resolved away in `BuildAst.cpp`) — avoid reusing an enum member's name
for a variable, parameter, or function in the same file; doing so is
not diagnosed, and the enum constant always wins at any later use of
that name.

## Bitwise operators

`&`, `^`, `|`, `<<`, `>>`, `~` sit at the same relative precedence real
C puts them: shifts bind tighter than relational comparisons but
looser than `+`/`-`; `&`/`^`/`|` bind looser than equality but tighter
than `&&` (see `grammar/CParser.g4`'s own `condExpr`). Every backend
implements all of them, but not always with a single matching native
instruction — a backend missing some piece of hardware synthesizes it
from what it does have (e.g. PDP-7 has no hardware `or` and no
variable-count shift instruction, so `|` and `<<`/`>>` are built from
instructions it does have — see `Pdp7Codegen.cpp`'s own comments at
each one).

## Placement

A function or global declared with a trailing `@ addr` (e.g.
`int vector() @ 020 { ... }`) pins its entry point/storage to an exact
spot instead of wherever the backend would otherwise lay it out —
meant for things a real machine's own hardware fixes in place
regardless of what the compiler would pick, like a kernel's interrupt/
trap vector cells. See `docs/ir.md`'s own "Fixed absolute placement"
for exactly what `addr` means and how each backend applies it.

## Inline asm

`asm { ... }` is an escape hatch, not a parsed sub-language: the raw
text between the braces is emitted into the generated assembly exactly
as written, with no attempt by rcc to understand it — meant for real
hardware instructions this C subset has no portable expression for at
all (e.g. the PDP-7 disk controller's raw IOT instructions). It's
target-specific by construction (the text has to already be valid
assembly for whichever `--target` is compiling it), has no return
value, and it's the caller's own job to leave registers/the
accumulator in a sane state around it — rcc doesn't track what an
`asm` block does to them.

## Preprocessor

A small, from-scratch text-level preprocessor runs line-by-line over
the raw source, entirely before the ANTLR lexer/parser ever see it:
`#include "file"` / `#include <file>`, `#define NAME` / `#define NAME
value` (object-like and function-like, including variadic macros,
`#` stringizing, and `##` token pasting), `#undef`, `#ifdef`/`#ifndef`/`#if`/
`#elif`/`#else`/`#endif` (`#if`/`#elif` evaluate a small integer-
constant-expression language: `defined`, integer literals, `! - + * /
% == != < <= > >= && ||`, parens, real C precedence — an undefined
identifier evaluates to `0`, same as real cpp), `#pragma once`
(other `#pragma` forms are silently ignored), `#error message`,
`#warning message`, and `#line NUM ["file"]` (retargets the file/line
reported in diagnostics and `__LINE__`/`__FILE__` for everything after
it, until the next `#line` or the end of the current file).
`__LINE__`/`__FILE__`/`__DATE__`/`__TIME__` are predefined, alongside a
target macro for whichever backend is active (`__PDP7__`, `__8086__`,
etc. — see `main.cpp`'s own table). `__LINE__` substitutes as a plain
integer literal; `__FILE__`/`__DATE__`/`__TIME__` substitute as a real
string literal, valid anywhere one is (`int p; p = __FILE__;`).

Macro names are expanded only in preprocessing tokens, not inside
string/character literals or comments. `#if`/`#ifdef` nesting must
balance within a single included file, and downstream line numbers after
an `#include` are approximate.

## COFF object files

rcc's own `--format=coff` is a real relocatable object (one `.text`
section, one `.bss` size reservation, a symbol table, a relocation
table). For every target except the `i386-windows` hosted triple, it's
a **deliberate, non-standard** choice: COFF has no historical tie to
any target this project supports (PDP-7 predates it entirely; real
16-bit 8086/DOS tooling used OMF, not COFF). Each such object uses its
own custom, unregistered `Machine` value, which existing tooling's
COFF readers (`objdump` included) reject outright as "not a valid
object file" purely because of that one unrecognized field — this is
expected, not a bug: rcc's COFF is one of several interchangeable
object-container formats it defines purely for its own `--link` step
(alongside ELF/Mach-O/a.out), never meant for interop with a real,
independent COFF toolchain. `i386-windows` is the exception: it uses
the standard `IMAGE_FILE_MACHINE_I386` value and standard COFF
relocation/symbol records, producing real objects and `.lib` libraries
that interoperate with other Windows toolchains — see
[`docs/using.md`](using.md)'s "Hosted executables" section. See
`src/fileformat/coff/CoffFormat.h`'s own header comment for the full
reasoning.

## Extern declarations

Under real separate compilation (`--format=coff`/`elf`/`macho`/`aout`,
each file its own, genuinely independent compile — see
[`docs/using.md`](using.md)'s "Link mode"), a plain, **unambiguous**
reference to a name this file doesn't itself define still just works
with no declaration at all: a call (`f(...)`) can only mean a function,
a plain variable read/write (`x`, `x = 1`) can only mean data, so the
first one of either seen in the file silently creates the right kind
of external placeholder (resolved for real later, at `--link` time).

Taking a bare name's own *address* — `&name`, or naming it directly as
an array-initializer element (`int t[] = {name};`, the dispatch-table
idiom) — is different: there's no way to tell a function address from
a data address apart from the syntax alone, so guessing which kind a
bare name refers to would be wrong exactly half the time, producing a
confusing `undefined symbol 'FN_x'`/`'G_x'` from `--link`, not from the
compile that actually made the wrong call. So `&name`/an array-initializer
element require the name to already be known by the time they're
reached — defined in this file, already called/read earlier in it, or
explicitly declared with `extern`:

```c
extern int iget(int inum);   // a function defined elsewhere
extern int inode[16];        // an array defined elsewhere
extern int nextId;           // a scalar defined elsewhere

int dispatch[2] = {iget, inode};  // now unambiguous - no guessing

int main() {
    return &nextId;
}
```

An `extern` function declaration is always bodyless (`;`, never a
block) and takes no `@addr` (that's meaningful only for a real
definition, which lives in whichever file actually has one). An
`extern` global (scalar or array) likewise takes no initializer and no
`@addr`, and an array one states its real size directly (`[16]` above)
since there's no initializer here to infer it from. Declaring the same
name both `extern` and for-real-in-this-file, or `extern` twice, is a
compile error, same as any other redeclaration.

This has no effect at all outside real separate compilation (a plain
`--format=asm`/`raw`/... whole-program compile already sees every name
in the one file it's given, so there's never anything ambiguous to
declare).

## Static declarations

`static` has two different uses, same as real C:

A file-scope `static` on a global or a function gives it **internal
linkage**: it's still an ordinary global/function in this file, but
it's never resolved against another file's `extern` declaration of the
same name, and never collides with another file's own same-named
`static` under real separate compilation (`--format=coff`/`elf`/`macho`
+ `--link`) — each file's `static int counter;`/`static int helper()
{...}` is its own, independent symbol, even if another file happens to
declare one with the identical name. (`extern` and `static` are
mutually exclusive on the same declaration — a name defined in this
file is either visible to other files or it isn't.)

A block-scope `static` local has **static storage duration**: one
persistent storage location that exists for the whole program, not a
fresh one per call, initialized exactly once (before `main` runs, like
a global) rather than every time its declaration statement executes.

```c
int next() {
    static int calls = 0;  // initialized once, not on every call
    calls = calls + 1;
    return calls;
}
```

Calling `next()` three times returns `1`, `2`, `3` — `calls` keeps its
value across calls, unlike an ordinary (non-`static`) local, which is a
fresh, uninitialized frame slot every time its enclosing block is
entered. A `static` local's own name still obeys ordinary block
scoping (invisible outside its function, shadowable the same way a
plain local is) — only its *storage* behaves like a global's.

A `static` local's initializer, same as a global's, must be a
plain (optionally negated) integer literal or array-initializer list —
it's evaluated once, at compile time, not as a runtime expression on
each call, so there's no runtime value available to initialize it
from. A `static` local with no initializer starts at zero, same as an
uninitialized global. The same restrictions a global has otherwise
(see "Structs" above) apply here too — e.g. no struct initializer, no
array of struct.

## Building / Usage

Building rcc from source, and every compile-mode/link-mode
command-line switch, are documented separately — see
[`docs/building.md`](building.md) and [`docs/using.md`](using.md).
