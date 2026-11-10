# Known bugs

Defects that are understood but not yet fixed. Each entry gives the
affected code, the trigger, and the wrong behavior.

## Integer literals at or above 2^63 fail to parse

Affects every target (`src/ast/BuildAst.cpp`).

Literal text is converted with `std::stol`, so a value such as
`18446744065119617025ULL` aborts compilation with a `stol` error
instead of wrapping to its unsigned value. Values built from smaller
constants (`4611686018427387904ULL + 4611686018427387904ULL`) work.

## sparc: wide truthiness test never combines the two words

Affects the `sparc` target (`src/codegen/sparc/SparcCodegen.cpp`).

For a `long long` condition in `if`, `while`, `?:` and the logical
operators, the backend emits `or t0,t0,t1`. SPARC assembly puts the
destination last, so this writes `t1` and leaves `t0` holding only the
low word. A wide value whose low word is zero but whose high word is
not (for example `1LL << 32`) tests false.

## pdp7: an `@addr`-placed function's own return-address reload breaks when entered from outside the normal call graph

Affects the `pdp7` target (`src/codegen/pdp7/Pdp7Codegen.cpp`).

A function declared with `@addr` (e.g. `int f() @ 2000 { ... }`) is
pinned to that address, but its compiler-generated prologue reloads
its own return address relative to its own entry label the same way
every ordinary pcc-emitted call site sets it up beforehand. When the
only thing that ever transfers control to it is hand-written assembly
outside that call graph (e.g. a `jms`/`jmp` from a separately
assembled low-memory vector table, built with `pdp-assembler` and
combined with a `pcc --link`ed image), that reload reads stale/wrong
data and the function's own first real instructions never run:
execution instead jumps back to the address the external caller's own
sequence started at, re-running it in a loop. Confirmed directly on
the real `pdp7` simulator by single-stepping: PC correctly lands one
word past the placed function's entry (real compiled content is
there, not corrupted), but the next few instructions divert control
back into the external caller instead of continuing into the
function's own body.

Workaround: make the `@addr`-placed function's entire body a single
unconditional `asm { }` jump to a separate, ordinarily pcc-linked
(non-`@addr`) function that does the real work — only the jump
target's address needs to be fixed, so the broken self-reference is
never exercised.

## pdp7: `--gc-sections` drops an `@addr`-placed function with no caller in the kept call graph

Affects the `pdp7` target in combination with `--link --gc-sections`.

`--gc-sections` keeps a function only if something already in the
kept call graph calls it. An `@addr`-placed function meant to be
entered solely from outside pcc's own compiled output (e.g. a
hand-assembled vector table jumping to a fixed address) has no such
caller, so it gets discarded even though its placement is a real,
explicit root. Confirmed directly: the same `@addr`-placed function
and its fixed address are reachable with `--gc-sections` off, and
disappear from the linked image with it on. Workaround: link without
`--gc-sections` when a build relies on an externally entered
`@addr`-placed function.
