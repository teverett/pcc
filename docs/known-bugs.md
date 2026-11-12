# Known bugs

Defects that are understood but not yet fixed. Each entry gives the
affected code, the trigger, and the wrong behavior.

## PDP-7 codegen re-derives repeated sub-expressions and lowers small-constant comparisons generically, inflating code size 4-10x

Affects `src/codegen/pdp7/Pdp7Codegen.cpp` (and whichever optimizer passes
are expected to clean up after it - `LocalCSE.cpp`/`DeadStoreElimination.cpp`
don't catch these).

Measured directly: comparing real assembled word counts for the same
functions, hand-written PDP-7 assembly (UnixV0-C's own original Unix V0
kernel, `src-assem/`) vs. `rcc`-compiled C ports of those same functions
(`src-c/`), using `rcc --link --format=list` to get real linked addresses
and computing sizes from consecutive-address deltas. The generated code is
consistently 4-10x larger than the original for the same logic - not one or
two bad translations, a systemic ratio across unrelated functions (`swap`
50→311 words, `pget` 69→447, `namei` 48→288, `.open` 26→234, `.status`
26→243, `fassign` 25→250, `awake` 16→84, etc.).

Root-caused by reading `awake()`'s full generated assembly line-by-line
(the smallest case: a 10-iteration loop with one array-indexed comparison)
against the hand-written original:

1. **Every array reference re-derives its own base address from scratch,
   every time it's used**, instead of computing it once and reusing the
   result within the same function. `ulist[...]` is touched 3 times in
   `awake()` alone; each touch repeats:
   ```
   law G_ulist
   and 8191        ; strip law's own opcode bits off the address
   ```
   (the extra `and`-mask itself is because `law` loads its own full raw
   instruction word, not a clean operand - a related, already-understood
   target quirk, not the bug being filed here). 6 of `awake()`'s 84 words
   go to this alone; it recurs at every array access in every function.
2. **A comparison against a small, provably-safe constant (`i < 10` in a
   `for` loop with `i` known non-negative) lowers via the fully general
   signed-comparison sequence** - a separate "is the variable itself
   negative" check, then a full two's-complement subtract with its own
   sign check, stitched together with several unconditional jumps - 15
   words, where a specialized "compare against a known-small non-negative
   constant" form could do it in 2-3.

None of these produce incorrect output - each is a missed optimization,
not a correctness bug - but together they compound: pattern 1 and 2 recur
at essentially every array access and every bounded comparison in a real
program, and `rcc` has no cross-call-site code sharing (each call site
gets its own fully inlined copy, unlike hand-written code that factors
shared sequences into subroutines), so the waste multiplies rather than
amortizing. This is the main reason UnixV0-C's own ported kernel can't fit
the PDP-7's 8192-word address space in one link even with
`--gc-sections`: trimming the *C source* has weak leverage when the bloat
is structural in how the codegen lowers these two extremely common
patterns, not localized to specific translations.

Pattern 1 is architecturally rooted in the IR (there is no address-CSE/
value-numbering pass shared across backends - `LocalCSE.cpp` only matches
textually-identical sub-expressions, so `ulist[i]`/`ulist[j]`/`ulist[k]`
never unify), though PDP-7 pays the highest cost per re-derivation because
`law` bakes its own opcode bits into the loaded address and needs an
extra masking `and`. Pattern 2 is PDP-7-specific: the PDP-7 has no
condition-code/flags register, so every signed `<` is synthesized from
sign-bit tests plus a two's-complement subtract regardless of operand
size, unlike i386/6502 which get this for free from a native compare
instruction.

(A third pattern originally filed here - uninitialized scalar locals
being zeroed twice, once by a defensive zero-store `Pdp7Codegen.cpp`
inserted for every uninitialized Local and again by any real initializer
- was PDP-7-codegen-specific, not an IR/optimizer gap, and has been
fixed by deleting that defensive zero-store.)

## `tests/run_tests.sh` never runs in CI

Affects `.github/workflows/ci.yml`.

The `macos` job's "Install Python simulator packages (py65, z80-python)"
step has a comment saying "`make test` (below) drives real, independent
CPU cores for the 6502/65c02 (py65) and 8080/z80 (z80-python) targets",
but no job invokes `make test` or `tests/run_tests.sh` anywhere in the
workflow, so that suite - and the py65/z80-python packages installed for
it - never actually run in CI.

