---
name: add-rcc-platform
description: Add support for a new processor or platform (target) to rcc, the retargetable C compiler in this repo. Use whenever the user asks to add, port, or support a new CPU, machine, console, computer, or OS target in rcc, even if they only name the hardware ("add the Z8000", "support the Amiga", "rcc for the Apple IIgs"). Covers the codegen backend and assembler, tests, runtime library, file format, linker factory, example program, Makefiles, CI, platformtests.yml, and docs.
---

# Adding a platform to rcc

A platform addition spans many files, and it is easy to finish the compiler and forget the
runtime, the example, CI or the docs. Work through the checklist in order and tick each item off.
Use the update_plan task list to track it.

Before starting, read `AGENTS.md` in the repo root and obey it. The rules that matter here:

- Never run `git commit` or `git push`. Leave changes in the working tree for the user to review.
- Every new feature needs at least two tests.
- Fix every C/C++ warning.
- Docs describe how the code works now, never history or bug fixes. Comments only where logic is
  non-obvious.
- Log any bug found in `docs/known-bugs.md`, and remove the entry when it is fixed.
- Do not create boot loaders or boot disks. rcc is a general-purpose multitarget compiler and linker.
- Sync to lumpy at the end of the turn.

## 0. Decide the shape of the platform

Settle these before writing code, asking the user only about what you cannot infer or research:

- **Processor vs. machine.** A processor target (`6809`, `z80`) is separate from machine platforms
  built on it (`6809-coco`, `z80-trs80`, `z80-coleco`). Platforms are named `<cpu>-<machine>`, as in
  the `--platform=6502-c64` and `--platform=6809-decb` examples. Add the processor first.
- **Closest existing backend.** Pick the sibling architecture to copy from (same family, word size,
  endianness) and study its `src/codegen/<cpu>/` directory rather than starting blank.
- **Output format.** Does the machine have a native executable or media format (a load file, ROM
  image, disk image)? If so it gets a `--format`.
- **Runtime library.** Add one if the machine has enough documented I/O (a text screen, a serial
  port, a BIOS call) to implement `putc`-style output. Once there is one, it covers everything the
  platform offers (see step 3). If it cannot be done reliably, skip it and say so in the final
  report instead of inventing hardware behaviour.
- **Simulator.** Look for a simulator or emulator that can run the example headlessly (pip package,
  Homebrew formula, qemu, simh). Verify it exists and runs before wiring it in.

## 1. Processor support (`src/codegen/<cpu>/`)

Mirror an existing backend: `<Cpu>Assembler.{h,cpp}` and `<Cpu>Codegen.{h,cpp}`. A reference for the
file layout is `src/codegen/m6809/` or `src/codegen/vax/`.

**Implement the processor's entire instruction set in the assembler**, not just the subset the
codegen emits today. Take the full opcode list from the vendor's programming manual or datasheet:
every mnemonic, every addressing mode, and every operand-size variant, including privileged,
system, I/O, string, bit-manipulation and floating-point instructions where the architecture has
them. Rcc also assembles hand-written assembly (and runtime libraries use it), so a partial
instruction set leaves gaps that surface later as unexplained assembler errors.

- Build a checklist from the manual before writing code, and work through it until every
  instruction is encoded. Mark anything genuinely undefined, reserved or undocumented, and say so
  in the final report instead of silently dropping it.
- Cover each instruction with assembler tests that check exact encoded bytes, taken from the
  manual's opcode tables. Test every addressing mode and operand size, not just one representative
  per mnemonic. Keep the total at two tests or more per instruction group.
- Where there are CPU variants (such as 6502/65c02/2a03, or 8080/z80), implement each variant's
  full set, and make the assembler reject instructions that the selected variant does not have.
- The codegen may still use only the instructions it needs. Full coverage is a requirement of the
  assembler and its tests.

- Register the codegen in `src/main.cpp`: the `#include`, the target-name list near the top, the
  codegen factory (`if (target == "...") return std::make_unique<...>()`), the linker factory, and
  the predefined macro such as `__6809__`.
- Add `src/linker/factory/<Cpu>LinkerFactory.{h,cpp}` and include it in `src/main.cpp`. Copy the
  nearest sibling, including its entry stub.
- Add `src/fileformat/FileFormat.cpp`'s machine-number entry if the new CPU needs one.
- Add the new `.cpp` files to the root `Makefile`: `BACKEND_DIRS`, the source lists, and the factory
  list.
- If the assembler is parsed through the ANTLR grammar (`grammar/RccAsm.g4`), extend the grammar and
  regenerate. Build the assembler test the way `m6809_assembler_test` is built.

### Tests for the processor

Two tests is the floor from `AGENTS.md`, not the goal. A new processor needs a comprehensive suite
that a regression in any part of the backend would fail.

- `tests/<cpu>/*.c`: target-specific programs covering the backend comprehensively. Cover each of
  these areas, with several cases per area:
  - every C integer type and width, signed and unsigned, including arithmetic, comparison,
    conversion, and overflow and wraparound at the type limits
  - multiply, divide and modulo at every width, including the wide (`long long`) paths and
    high-bit operands
  - shifts and rotates by constant and variable counts, bitwise operators, and compound assignment
  - control flow: branches in range and out of range (far branches), loops, `switch`, and
    `break`/`continue`
  - calls: argument counts, mixed-width arguments, return values of each width, recursion, function
    pointers, and stack use
  - pointers, arrays, structs, globals, statics and initialisers, plus addressing across segment,
    bank or page boundaries where the architecture has them
  - each addressing mode and register-pressure case the codegen can produce
- Run the shared suite in `tests/*.c` against the new target too, and justify each exclusion.
- Each test checks a result, whether through an exit value or a printed or stored value, and not
  only that compilation succeeded. Where an emulator exists, execution is the check.
- `tests/<cpu>_assembler_test.cpp` for the assembler, with a `Makefile` rule next to
  `m6809_assembler_test` and entries in both `test` and `test-hosted-internal`. It must exercise
  the full instruction set, as described above.
- `tests/run_tests.sh`: add the target to the usage line, the tool-check `case`, the "unknown
  --target" list, and a `check_<cpu>()` that runs the compiled program in an emulator. Add the same
  target to `tests/run_link_tests.sh` if the sibling targets appear there.
- If no verified emulator exists, make `run_tests.sh` compile-only for that target, say that
  execution is unverified, and log the gap in `docs/known-bugs.md`. Do not invent a placeholder
  emulator package.
- Add exclusions in `run_tests.sh` for any shared test the target legitimately cannot pass (for
  example `types_longlong_divmod.c`), with a reason next to each.

## 2. File format (`src/fileformat/<fmt>/`)

Only when the platform has a native format. Add `<Fmt>Format.{h,cpp}` with a reader and a writer,
modelled on `decb/`, `prg/`, `cmd/` or `colecorom/`.

- Add an enum case to `FileFormatKind` in `FileFormat.h` and wire it into every switch in
  `FileFormat.cpp`: the writer, the reader, the name-to-kind parse, and the kind-to-name.
- Register the platform string (`<cpu>-<fmt>`) in `src/main.cpp` so `--platform` and
  `--list-platforms` show it.
- Add `tests/<fmt>_format_test.cpp` with a `Makefile` rule, and add it to both `test` and
  `test-hosted-internal`. The test suite must be comprehensive: two tests is the floor from
  `AGENTS.md`, not the goal. Cover:
  - byte-exact output for the header, trailer and every record or block type the format defines
  - every field's encoding, including endianness, checksums, lengths, padding and alignment
  - the writer's boundaries: empty input, a single byte, exactly one block, one byte over a block,
    the maximum size, and addresses at the limits (and rejection of out-of-range input with a
    clear error)
  - multiple segments, gaps between segments, and entry-point or load-address handling
  - a writer-to-reader round trip that reproduces the original image
  - the reader rejecting malformed input: truncated files, bad magic or checksums, and bad lengths
  - at least one byte sequence taken from the format's specification or a real tool's output, so
    the test does not just agree with the implementation
  Add an end-to-end check as well: link a small program with `--format=<fmt>` and verify the output
  with the reader.
- Add the source file to the root `Makefile`'s `FILEFORMAT_DIRS` and source lists.

## 3. Runtime library (`src/rt/<machine>/`)

Only when step 0 said it is feasible. Follow `src/rt/coco/`:

- **Implement the entire runtime library: every function the platform makes available**, not just
  the output call the example needs. Take the list from the machine's documentation (ROM/BIOS/OS
  call tables, hardware register maps, system manuals) and cover each area the hardware has:
  console and screen output, keyboard and joystick input, serial and parallel I/O, disk or tape
  access, sound, graphics, timers, memory and system services. Work from a checklist, as with the
  instruction set, and implement every item. List anything undocumented, unreliable or impossible
  to express in rcc's C subset in the final report instead of silently omitting it.
- Declare every function in `<machine>.h`, and add tests of at least two per area. These can be
  compile and link checks, or runs in the simulator when one exists.
- `<machine>.h`, `<machine>.c` and a `Makefile` that produces `<machine>.lib` through rcc itself.
  Split the implementation across several `.c` files when the library is large.
- Add the directory to `SUBDIRS` in `src/rt/Makefile`, and update the list of names in its header
  comment.
- Keep the runtime to documented hardware behaviour. Note machine quirks that affect callers, such
  as character encodings or screen layout, in the docs.

## 4. Example (`examples/<cpu>-<machine>/`)

- `main.c`, a short hello-world that uses the runtime.
- `Makefile` modelled on `examples/6809-coco/Makefile` (no simulator) or `examples/6502-c64/Makefile`
  (simulator). It includes `../examples.mk`, builds the runtime lib via a sub-make, links with
  `--platform=<cpu>-<fmt>`, and defines `all`, `test` and `clean`.
- `README.md` describing what the example is and how to run it.
- **If a simulator can run it**, add a `test.py` (or the equivalent) that loads the output and checks
  the result, and make the Makefile's `test` target run it. Otherwise `test` only checks that the
  output files exist, with a comment saying no emulator is wired in.
- Register the example in `examples/Makefile`: the `.PHONY` list, the `all` prerequisites, and a
  rule. Add an entry to `examples/README.md`.

## 5. Build, CI and workflows

- Root `Makefile`: everything from steps 1 to 3, including the `test` and `test-hosted-internal`
  prerequisite lists.
- `.github/workflows/ci.yml`: the CI runs on macOS, Linux and Windows. If a simulator is needed for
  `tests/run_tests.sh`, install it in the existing Homebrew/pip step.
- `.github/workflows/platformtests.yml`: add `make -C examples/<cpu>-<machine> test` to the
  macOS-only example list, and add an install step for the simulator (`pip3 install
  --break-system-packages <pkg>` or `brew install <pkg>`) if the example needs one. Update the
  comment that lists which examples have no emulator.

## 6. Documentation

- `docs/platforms.md`: a section for the processor and each machine (instruction set coverage, ABI,
  memory map, runtime, formats). Match the existing entries' structure.
- `docs/using.md`: the target and format tables, and the `--platform` examples if relevant.
- `README.md`: the "Supported `--target` values" list.
- `docs/known-bugs.md`: log any limitation or defect found while porting.
- Keep all of it present-tense and about how things work, with no "added in" history.

## 7. Verify before reporting

1. Build with the root `Makefile`. There must be no warnings.
2. Run `tests/run_tests.sh build/bin/rcc --target=<cpu>`, and the format and assembler tests. All
   of them must pass, with no test skipped or weakened to get there.
   Check the instruction-set checklist from step 1: every instruction in the manual is either
   encoded and tested or listed as undefined in the report.
3. Run `make -C examples/<cpu>-<machine> test`.
4. Check the runtime-library checklist: every function the platform offers is implemented and
   declared, or listed in the report with the reason.
5. Run `rcc --list-platforms` and confirm the new entries appear.
6. Sync to lumpy.

The final report should name what was added, what was verified in a simulator versus compile-only,
and anything skipped (for example no runtime library, or no emulator) with the reason. Leave the
changes uncommitted.
