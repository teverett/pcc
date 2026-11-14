---
name: ensure-completeness
description: Audit and complete rcc's coverage across every target. For each supported processor, make sure every instruction in the manual is implemented in the assembler, can be used from a C `asm { }` block, that unsupported or invalid instructions fail with an error; and make sure every runtime library under src/rt is as complete as the platform allows. Assembler and file format tests belong to the test-coverage skill. Use whenever the user asks to check, audit, verify or ensure completeness, coverage, "every instruction", "every function", missing instructions, gaps in a target or runtime library, or asks whether a target is "finished", even if they don't mention asm blocks.
---

# Ensuring completeness

rcc's backends share one assembly grammar, and each target's in-process assembler decides what a
mnemonic and its operands mean. `asm { ... }` copies its text verbatim into the generated assembly
(see `docs/language.md`, "Inline asm"), so the assembler is the only thing standing between a C
program and the hardware. An instruction the assembler lacks is therefore unusable from C, and an
instruction it wrongly accepts produces silent garbage. This skill closes both gaps for every
processor, and then does the equivalent for every runtime library.

`AGENTS.md` in the repo root is the source of truth for repo rules. Follow it: never run
`git commit` or `git push`; every new feature or bug fix gets at least two tests; fix every C and
C++ warning; write docs that describe how things work now, never history; log any bug in
`docs/known-bugs.md` and remove the entry when it is fixed; no boot loaders or boot disks.

## Scope

By default audit every target. If the user names a processor or machine, audit only that one. Get
the target list from `rcc --list-platforms` and the `src/codegen/*/` directories, and the runtime
libraries from `src/rt/*/`. Assembler and file format tests are covered by the separate `test-coverage` skill. Work one target at a time and finish it before starting the next, so
partial progress is real progress.

This is a long job. Track the targets in the task list, and record each target's state (complete,
partial, blocked) as you go so the work can resume.

## Part 1: processors

For each processor, do the following in order.

### 1. Inventory what exists

- Read `src/codegen/<cpu>/<Cpu>Assembler.{h,cpp}`, and note the CPU variants it handles (for
  example 6502 / 65c02 / 2a03, or 8080 / z80).
- Check what mechanism handles `asm { }` text for this target. Some targets may only pass text through to
  an external stage with no in-process assembler. Note it: a pass-through can neither validate
  nor reject anything.
- Find the existing tests (`tests/<cpu>/`, `tests/<cpu>_assembler_test.cpp`) and what they cover.
- pdp7 and similar targets treat mnemonics as symbols whose values OR together instead of selecting
  an instruction (see `docs/asm.md`). For such a target, "every instruction" means every
  instruction word and every operand-field combination the machine defines, and "unsupported"
  means a symbol or combination that is not defined.

### 2. Build the instruction manifest

Write a manifest from the vendor's programming manual or datasheet, never from the current
assembler source (that would just confirm what's already there). Put it in `tests/isa/<cpu>.txt`,
or follow whatever layout the repo already has if one exists. One entry per mnemonic, listing:

- each addressing mode and operand form
- operand size variants
- which CPU variant has it
- the expected encoding of at least one example, taken from the manual's opcode tables

Include privileged, system, I/O, string, bit-manipulation, coprocessor and floating-point
instructions where the architecture has them. For an instruction that is undefined, reserved or
undocumented, record it as such under its own heading, so "not implemented" and "does not exist"
stay distinct. If the manual isn't available, search for it and ask the user for it before
guessing. Do not reconstruct opcode tables from memory.

### 3. Diff the manifest against the assembler

List every manifest entry the assembler does not encode, or encodes wrongly. Implement them all,
in the assembler (`<Cpu>Assembler.cpp`) and, if the grammar needs it, in `grammar/RccAsm.g4`,
regenerating as the project does. Keep CPU variants honest: an instruction that exists only on a
later variant is not accepted for an earlier one.

### 4. Prove each instruction works from C

For each instruction (or tight group sharing one encoding path), add a test that uses it through
an `asm { }` block in a C file, so the test exercises the same route a user's program takes:
C source, then the compiler, then the assembler, then the linked bytes.

- Put C tests in `tests/<cpu>/` in the style of `tests/test_asm_noop.c`, guarded by the target's
  predefined macro (`__6809__` and so on).
- Where an emulator exists for the target, execute the instruction and check an observable result.
  Where it does not, link to `--format=raw` and compare the emitted bytes to the manifest's
  expected encoding. Never leave a test that only checks that compilation succeeded.
- Cover every addressing mode and size in the manifest, not one example per mnemonic.
- The direct assembler test for each platform (`tests/<cpu>_assembler_test.cpp`) belongs to the
  `test-coverage` skill. Run it after this step, using the manifest you just built.

### 5. Prove unsupported instructions fail

An assembler that accepts anything is worse than one that accepts less. For each target, add
negative tests that check rcc fails, with a non-zero exit status and an error message that names the
offending text (line number included, as `BuildAst.cpp` already does for asm errors):

- an unknown mnemonic
- a real mnemonic with an addressing mode it does not have
- wrong operand counts, wrong operand types and wrong register names
- operand values out of range (immediates, displacements, branch distances, register numbers)
- an instruction that belongs to a different CPU variant than the selected one (for example a
  65c02-only instruction on `6502`)
- instructions from a sibling architecture, which makes a good generic check (a Z80 instruction
  on `8080`)

Each negative test asserts the failure and the message, and also that no output file is
produced. A test that only observes "it didn't crash" does not count.

### 6. Verify

- Build the project with no warnings, and run the target's test suite plus the new tests. All
  must pass, with nothing skipped or weakened to get there.
- Re-diff the manifest: every entry is either implemented and tested, or listed as undefined with
  the reason.
- Update `docs/platforms.md` if the target's supported instruction set description changed, so the
  docs state which variants and instruction groups are covered. Describe what is, with no history.

## Part 2: runtime libraries

For each directory in `src/rt/` (and each machine that should have one but doesn't, like the empty
`cpc/`), make the library as complete as the platform allows.

1. **Build the function list** from the machine's own documentation: ROM, BIOS or OS call tables,
   hardware register maps and system manuals. Group it by area: console and screen, keyboard and
   joystick, serial and parallel I/O, disk and tape, sound, graphics, timers, memory and system
   services. Compare the list with `<machine>.h` and `<machine>.c`, and write down the gaps.
2. **Implement every missing function** that rcc's C subset can express, and declare it in the
   header. Use documented behaviour only. Where a function genuinely can't be written reliably (an
   undocumented timing dependency, hardware that can't be exercised, something the C subset can't
   say), leave it out and record why, rather than inventing hardware behaviour. Escape-hatch
   `asm { }` is fine inside the runtime, and the instruction coverage from Part 1 is what makes that
   possible.
3. **Test it.** At least two tests per area. Use the machine's simulator when the example setup has
   one; otherwise, assemble and link against the library and check that it builds and that the symbols
   resolve. Add them to the relevant example or `tests/` runner.
4. Check that `src/rt/Makefile`'s `SUBDIRS`, the library's own `Makefile`, and the machine's example
   all build with the new functions.
5. Update the machine's docs in `docs/platforms.md` with what the library covers.

## Report

When done, give a table of targets and libraries with: instructions in the manifest, implemented,
tested through `asm { }`, negative tests added, and any entries listed as undefined or blocked,
with the reason. Be direct about what was only verified by byte comparison rather than execution, and
about anything you could not complete (a missing manual, a missing simulator, an undocumented
instruction). Leave all changes uncommitted.
