# Building

```
make                # uses Homebrew's antlr4-cpp-runtime
make test           # the preprocessor/archive/Mach-O/hosted-linker/pic16+pic18 unit
                     # tests, then tests/run_tests.sh against every target that has one
```

`make test` runs `tests/run_tests.sh` against `pdp7`, `8086`, `6502`,
`65c02`, `68000`, `mc68328`, `8080`, `z80`, `8051`, `stm32`, and
`i386` (each one's own tool, where it needs a separate one, is
documented in `docs/platforms.md`'s own per-target entry - a missing
tool fails loudly rather than being silently skipped).

`tests/run_tests.sh` also has checks for `pdp11`, `r10000`, `powerpc`,
`cortex-m3`, `armv7a`, `armv8a`, and `8008` that `make test` doesn't
run; invoke any of these directly with `tests/run_tests.sh
build/bin/pcc --target=<name>`. Their software-identical aliases
(`r4000`, `cortex-m4`/`cortex-m7`, like `8088`'s alias of `8086`
above) aren't separately runnable here - same backend, provably
byte-identical output. `stm8`, `m16c`, `sparc`, `pic16`, and `pic18`
have no `tests/run_tests.sh` check at all - those backends were
verified directly against a real simulator or by hand-decoding
assembled output during development instead (see
`docs/platforms.md`).

The generated C++ lexer/parser code (`src/generated/`, from
`grammar/*.g4`) is checked into git - `make` never invokes the real
ANTLR tool itself, so building pcc needs only the ANTLR4 C++
*runtime* (headers/lib), not Java or the ANTLR jar. See the
Makefile's own header comment for how to point it at a non-Homebrew
runtime install instead.

After editing a `grammar/*.g4` file, regenerate `src/generated/` by
hand with `scripts/generate-grammars.sh` (or `make generate`, which
just runs that script), review the diff, and commit both together.
