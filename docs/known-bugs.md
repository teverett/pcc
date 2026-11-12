# Known bugs

Defects that are understood but not yet fixed. Each entry gives the
affected code, the trigger, and the wrong behavior.

## PDP-7 codegen lowers small-constant comparisons generically, inflating code size

Affects `src/codegen/pdp7/Pdp7Codegen.cpp`.

Measured directly: comparing real assembled word counts for the same
functions, hand-written PDP-7 assembly (UnixV0-C's own original Unix V0
kernel, `src-assem/`) vs. `rcc`-compiled C ports of those same functions
(`src-c/`), using `rcc --link --format=list` to get real linked addresses
and computing sizes from consecutive-address deltas. The generated code is
consistently larger than the original for the same logic - not one or two
bad translations, a systemic pattern across unrelated functions.

Root-caused by reading `awake()`'s full generated assembly line-by-line
(a 10-iteration loop with one array-indexed comparison) against the
hand-written original: **a comparison against a small, provably-safe
constant (`i < 10` in a `for` loop with `i` known non-negative) lowers via
the fully general signed-comparison sequence** - a separate "is the
variable itself negative" check, then a full two's-complement subtract
with its own sign check, stitched together with several unconditional
jumps - 15 words, where a specialized "compare against a known-small
non-negative constant" form could do it in 2-3.

This is PDP-7-specific, not an IR/optimizer gap other backends also hit:
the PDP-7 has no condition-code/flags register, so every signed `<` is
synthesized from sign-bit tests plus a two's-complement subtract
regardless of operand size, unlike i386/6502 which get this for free from
a native compare instruction. `rcc` also has no cross-call-site code
sharing (each call site gets its own fully inlined copy, unlike
hand-written code that factors shared sequences into subroutines), so this
waste multiplies rather than amortizing across a real program - a
contributing reason UnixV0-C's own ported kernel struggles to fit the
PDP-7's 8192-word address space in one link even with `--gc-sections`.

(Two other patterns originally filed here - every array reference
re-deriving its own base address from scratch at each use, and
uninitialized scalar locals being zeroed twice - have since been fixed:
the first by the `AddressHoisting` optimizer pass
(`src/optimizers/address_hoisting/`, see `tests/run_pdp7_address_hoisting_tests.sh`),
which now computes a repeatedly-accessed array's base address once per
function instead of at every access, on every backend, not just pdp7; the
second by deleting `Pdp7Codegen.cpp`'s own defensive zero-store, which was
PDP-7-codegen-specific, not an IR/optimizer gap.)

## `tests/run_tests.sh` never runs in CI

Affects `.github/workflows/ci.yml`.

The `macos` job's "Install Python simulator packages (py65, z80-python)"
step has a comment saying "`make test` (below) drives real, independent
CPU cores for the 6502/65c02 (py65) and 8080/z80 (z80-python) targets",
but no job invokes `make test` or `tests/run_tests.sh` anywhere in the
workflow, so that suite - and the py65/z80-python packages installed for
it - never actually run in CI.

