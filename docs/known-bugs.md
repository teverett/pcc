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

## pdp7: `&global`/`&array[idx]` used as an ordinary value bakes `law`'s own opcode bits into the address

Affects the `pdp7` target (`src/codegen/pdp7/Pdp7Codegen.cpp`).

Taking the address of a global scalar or array element outside a
static initializer context (e.g. `p = &arr[i];`, or as an ordinary
function-call argument) materializes it with `law <symbol>` - but
`law`'s own real semantics on this target load AC with its *entire
raw encoded instruction word*, not a decoded/masked operand (`law N`
produces `0760000 | N`, confirmed directly by executing one and
reading AC back). Using it to load a symbol's real address bakes that
same `0760000` into the result, so any later arithmetic or indirect
dereference through the value lands on the wrong memory - e.g.
`p = &arr[i]; *p = x;` can silently write nowhere near `arr[i]`.
Static initializers (`int arrAddr[1] = {arr};`) are unaffected - that
path resolves the symbol via a real link-time relocation, not `law` -
so reading the clean address back out of one, then doing ordinary
arithmetic on it, is a correct, already-used-elsewhere workaround
(e.g. UnixV0-C's own `g_coldentryAddr`/`g_calTrapEntryAddr`). Caught
directly while porting UnixV0-C's `.fork`: `u_ulistp = &ulist[idx];`
produced a pointer whose indirect writes never reached `ulist` at all.
