# Building

```
make                # uses Homebrew's antlr4-cpp-runtime
make test           # runs tests/run_tests.sh against every target that has one (see below)
```

`make test` currently exercises `pdp7`, `8086`, `6502`, `65c02`,
`68000`, `mc68328`, `8080`, `z80`, `8051`, `stm32`, and `i386` - the
targets `tests/run_tests.sh` has a real hardware-simulator check for
(each one's own tool, where it needs a separate one, is documented in
`docs/platforms.md`'s own per-target entry - a missing tool fails
loudly rather than being silently skipped). To run just one of these
directly: `tests/run_tests.sh build/bin/pcc --target=<name>`. Several
more recently added targets (`pdp11`, `stm8`, `m16c`, `r10000`/
`r4000`, `cortex-m3`/`cortex-m4`/`cortex-m7`, `powerpc`, `sparc`,
`pic16`, `pic18`) were verified directly against a real simulator or
by hand-decoding assembled output during development (see
`docs/platforms.md`), but don't have a `tests/run_tests.sh` target
yet.

The generated C++ lexer/parser code (`src/generated/`, from
`grammar/*.g4`) is checked into git - `make` never invokes the real
ANTLR tool itself, so building pcc needs only the ANTLR4 C++
*runtime* (headers/lib), not Java or the ANTLR jar. See the
Makefile's own header comment for how to point it at a non-Homebrew
runtime install instead.

After editing a `grammar/*.g4` file, regenerate `src/generated/` by
hand with `scripts/generate-grammars.sh` (or `make generate`, which
just runs that script), review the diff, and commit both together.
