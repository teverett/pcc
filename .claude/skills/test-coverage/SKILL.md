---
name: test-coverage
description: Make sure every assembler (one per platform) and every file format in rcc has a working, comprehensive test. Use whenever the user asks to check, audit, add or improve tests for assemblers, instruction encoding, file formats or output formats (--format values, the directories under src/codegen and src/fileformat), asks which assemblers or formats lack tests, or wants this test coverage verified, even if they only say "are the formats tested?" or name one target or format such as 6809, elf, coff, aout, prg or decb. Also use after adding or changing an assembler or file format.
---

# Assembler and file format test coverage

Every platform has an assembler under `src/codegen/<cpu>/`, and every directory under
`src/fileformat/` is a format. Each needs a working test. Code that is only exercised through
whole-program links can emit wrong bytes for years, because nothing checks the bytes themselves.
This skill covers the two parts in turn: assemblers first, then file formats.

`AGENTS.md` in the repo root is the source of truth for repo rules. Follow it: never run
`git commit` or `git push`; every new feature or bug fix gets at least two tests (a floor, not the
goal here); fix every C and C++ warning; docs describe how things work now, never history; log any
bug in `docs/known-bugs.md` and remove the entry when it is fixed.

## Scope

By default cover every assembler and every format. If the user names one, cover only that one. Work
on one at a time and finish it before starting the next.

## Part 1: assemblers

List the assemblers (`src/codegen/*/*Assembler.cpp`) and compare them with
`tests/*_assembler_test.cpp`. At the time of writing, `m6809` is the only one with a direct test, so
most targets need one written. Where a target has no in-process assembler, say so in the report
instead of inventing a test.

For each platform, write `tests/<cpu>_assembler_test.cpp`, modelled on
`tests/m6809_assembler_test.cpp`. It drives the assembler directly and checks exact encoded bytes.

- **Source of expected bytes:** Use the instruction manifest at `tests/isa/<cpu>.txt` if the
  `ensure-completeness` skill has built one. Otherwise build the list from the vendor's manual or
  datasheet and not from the assembler source, which would only confirm what it already does. Cover
  every mnemonic, addressing mode and operand size, and every CPU variant.
- **Beyond single instructions:** relocation, forward references, label arithmetic, multi-line
  programs and range errors (immediates, displacements, branch distances).
- **Rejection:** unknown mnemonics, wrong addressing modes, wrong operand counts or types, and
  instructions from another variant must fail with an error message, and produce no output.
- **Make it work**, as defined in "Making a test work" below.
- **Fix what the tests find**, as described there.

## Part 2: file formats

Compare the directories under `src/fileformat/` with `tests/*_format_test.cpp` to find the formats
with no test. Several (for example `a7out`, `aout`, `coff`, `elf`, `dos33`, `prodos`, `ptr`, `raw`,
`rim`, `list`) may have none. Also check the formats that do have tests, since those may be thin.
Test files follow `tests/<fmt>_format_test.cpp`, as in `tests/decb_format_test.cpp`.

### For each format

1. **Read the format's specification**, or the real tool's documented output, and write down every
   structural element: header, trailer, each record or block type, checksums, lengths,
   endianness, padding and alignment, load address and entry point, and any size limits.
2. **Write or complete the test**, covering at least:
   - byte-exact output for every element above
   - the writer's boundaries: empty input, one byte, exactly one block, one byte over, maximum
     size, addresses at the limits, and rejection of out-of-range input with a clear error
   - multiple segments, gaps between them and entry-point handling
   - a writer-to-reader round trip that reproduces the original image, for formats with a reader
   - the reader rejecting truncated, corrupt and mis-sized input
   - at least one byte sequence from the specification or a real tool's output, so the test does
     not merely agree with the implementation
   - an end-to-end check: link a small program with `--format=<fmt>` and verify the output
3. **Write-only formats** have no reader to round-trip against. Test them against an independent
   decoder instead: a small checker in the test that parses the bytes according to the spec, or
   an external tool (such as `readelf` or `file`) when one is available on every CI platform.
4. **Make it work**, as defined in "Making a test work" below.
5. **Fix what the tests find**, as described there.

## Making a test work

A test counts as working only when all of the following hold, and you must check each one by
running it:

- it has a `Makefile` rule, and is in both the `test` and `test-hosted-internal` targets
- it builds without warnings and passes
- it fails when the code under test is deliberately broken. Change one encoding, byte or field
  temporarily, confirm the test goes red, then restore it. A test that can't fail proves nothing.
- CI runs it through those same targets (`ci.yml` on macOS, Linux and Windows)

Fix defects the tests find in the assembler or format code, and not by loosening the test. If one
can't be fixed, log it in `docs/known-bugs.md` and report it.

## Report

Give two tables. For assemblers: whether a test existed, how many instructions and variants it
covers now, whether rejection cases are tested, whether it was shown to fail when the code is
broken, and any defects found. For file formats: whether a test existed, what it covers now, whether
it was shown to fail when the code is broken, and any defects found. Be direct about anything you
couldn't complete (no manual or specification available, no in-process assembler, no independent
decoder for a write-only format). Leave all changes uncommitted.
