# Command-line switches

A build produces a single `rcc` binary (`build/bin/rcc`) that runs in
one of two modes:

- **Compile mode** (the default): turns one or more `.c` files into
  target assembly and, unless asked for raw assembly text, assembles
  that into a real binary format.
- **Link mode** (`--link`): combines separately-compiled `.o` files
  (COFF/ELF/Mach-O/a.out objects produced by `--format=coff`/`elf`/
  `macho`/`aout`) into one linked image.

See [`docs/platforms.md`](platforms.md) for the full list of targets
and what each one's own hardware/instruction set actually is.

## Compile mode

```
rcc [--target=<name>] [--format=asm|<target-specific>] [--out=file]
    [--optimize] [--dump-ir] [--dump-asm] file.c [file2.c ...]
```

| Switch | Meaning |
| --- | --- |
| `--target=<name>[-<host>]` | Which backend to compile for, and optionally which real OS the produced executable will run on. `<name>` default: `pdp7`. One of `pdp1`, `pdp7`, `8086`, `8088`, `6502`, `65c02`, `68000`, `mc68328`, `pdp11`, `pdp10`, `stm8`, `m16c`, `m30626fjpfp`, `8080`, `z80`, `8008`, `8051`, `8052`, `8031`, `8032`, `stm32`, `i386`, `pic16`, `pic18`, `r10000`, `r4000`, `cortex-m3`, `cortex-m4`, `cortex-m7`, `armv7a`, `armv8a`, `powerpc`, `sparc`, `vax`, `avr`, `alpha`, `saturn`, `tms320c6713` — see `docs/platforms.md`. Several of these are deliberate, software-identical aliases for a sibling target rather than separate backends (`8088`→`8086`, `8052`/`8031`/`8032`→`8051`, `r4000`→`r10000`, `cortex-m4`/`cortex-m7`→`cortex-m3`, `m30626fjpfp`→`m16c`) - see each one's own entry in `docs/platforms.md` and `src/main.cpp`'s own `makeBackend()` comment. `<host>` defaults to `none` (freestanding/bare metal) and is otherwise `linux`/`windows` (`i386` only) or `macos` (`armv8a` only) - see "Hosted executables" below. Run `rcc --list-triples` for every `<name>-<host>` combination actually supported. |
| `--list-triples` | Print every supported `<name>-<host>` triple, one per line, and exit. |
| `--format=<fmt>` | What to produce. `asm` (the default) means "just the generated assembly text, don't assemble it". Every target supports the same uniform set of real formats, each handed to that target's own in-process assembler - no external assembler tool needed for any of them: `raw` (a complete, directly-loadable/bootable whole program - `hex` instead, for `pic16`/`pic18`'s own Intel-HEX convention), and the five real object-file formats real separate compilation uses - `coff`, `elf`, `macho`, `aout` (each a genuine relocatable object with symbols/relocations, for a future `--link` step), and `objraw` (a bare, symbol-less dump of the assembled bytes, for inspection only - it can never be `--link`ed, since it has thrown away the very `fn_main` symbol every target's linker needs). The two oldest targets keep their own historical format names instead: `pdp7` supports `a7out`, `list`, `ptr`, `rim`, `coff` (word-oriented paper-tape/listing formats, from before the format-neutral object model existed); `8086`/`8088` support `bin`, `coff`, `elf`. Two targets support fewer formats than the uniform set, with no real object format at all: `saturn` supports only `raw`/`coff` (its own `--link` output is `flat`, not `raw` - see `docs/platforms.md`); `tms320c6713` supports only `raw` (a first pass, with no relocatable object format and so no `--link` support at all). `pdp1` also has no real object format and so no `--link` support at all: it supports only `list` (a debug listing) and `rim` (a bootable paper tape, its own loader protocol - not pdp7's `rim`). A non-object format (`raw`/`hex`/`bin`/`asm`, `pdp7`'s own `a7out`/`list`/`ptr`/`rim`, or `pdp1`'s own `list`/`rim`) takes exactly one `.c` file - only the real object formats support combining multiple independently-compiled files, via `--link` afterward. |
| `--out=file` | Where to write the result. Omit it to write to stdout; with an object format and no `--out`, output instead goes to `build/<name>.o` (one per input file). |
| `--optimize` | Run every registered IR optimizer (see `docs/optimization.md`) before codegen. Off by default. |
| `--dump-ir` | Also write each input file's intermediate representation to `build/<name>.ir`, for inspection (see `docs/ir.md`). |
| `--dump-asm` | Also write each input file's generated assembly to `build/<name><ext>`, where `<ext>` is this target's own extension - `.s` (pdp7 and saturn - both share this extension), `.asm` (8086/8088), `.s65`/`.s65c` (6502/65c02), `.s68`/`.s328` (68000/mc68328), `.s11` (pdp11), `.s10` (pdp10), `.s8m` (stm8), `.s16c` (m16c and m30626fjpfp - both share this extension), `.s80`/`.sz80`/`.s08` (8080/z80/8008), `.s51` (8051/8052/8031/8032), `.pic16`/`.pic18` (pic16/pic18), `.s32` (stm32), `.s386` (i386), `.sarm` (armv7a and cortex-m3/m4/m7 - both ARM backends share this extension), `.s64` (armv8a), `.smips` (r10000/r4000), `.sppc` (powerpc), `.ssparc` (sparc), `.svax` (vax), `.savr` (avr), `.salpha` (alpha), `.stms6713` (tms320c6713). |
| `-h`, `--help` | Print usage and exit. |

Real separate compilation is the only way to combine more than one
`.c` file into a final program: compile each file on its own with
`--format=coff` (or `elf`/`macho`/`aout` - one invocation, several
files, each compiled completely independently, one `build/<name>.o`
per file), then combine every `.o` with `--link` into the final
program.

### Hosted executables

For an i386 Linux executable, compile with the `i386-linux` triple so
external symbols and calls use the i386 C ABI (`--format` is then implied
as `elf`):

```sh
rcc --target=i386-linux --out=main.o main.c
rcc --link --target=i386-linux --out=app \
    main.o -L/usr/lib/i386-linux-gnu -lm -lc
```

Hosted links use rcc's internal i386 ELF writer and never invoke an
external linker. It supports static executable images, ordinary static
archives and i386 ELF shared libraries. Shared-library imports use ELF dynamic symbols and
PLT/GOT relocations; dynamic executables name `/lib/ld-linux.so.2` as
their interpreter. `-lc` links the real target libc and selects
`__libc_start_main` startup so libc initialization and termination run
normally, including buffered output flushing. Libc-free executables call
`main` directly and exit through the Linux i386 syscall ABI.
`-L<dir>` adds a library search directory and `-l<name>` prefers
`lib<name>.so`, then falls back to `lib<name>.a`. `-l:<filename>` selects
an exact filename, including versioned shared-library filenames such as
`libfoo.so.1`; positional `.so` and archive paths are also accepted.
Bounded GNU `INPUT`, `GROUP`, `AS_NEEDED`, and `OUTPUT_FORMAT` scripts and
versioned dynamic function imports are supported. See
[`hosted-linux.md`](hosted-linux.md) for input formats and limits.
Unresolved imports are rejected. `--linker=<path>` is unsupported and fails rather than falling
back. Omitting `--out` writes
`a.out`.

For 32-bit Windows PE executables and COFF `.lib` libraries, use the
`i386-windows` triple when compiling and linking (`--format` is then
implied as `coff`):

```sh
rcc --target=i386-windows --out=main.obj main.c
rcc --link --target=i386-windows --out=app.exe \
    main.obj -Lpath/to/libs -lfoo
```

Hosted links use rcc's internal PE32 writer. Ordinary static COFF
archives (`.lib` or `.a`) are selected lazily. Microsoft short import
objects and GNU import archives (`.dll.a`) supply named or ordinal DLL
imports, including `__imp_` data pointers. A positional i386 DLL or
`-l<name>` can also resolve exports directly from a real DLL.
`-L<dir>` adds a search directory; library lookup tries `<name>.dll`,
`<name>.lib`, `lib<name>.dll.a`, and `lib<name>.a`. On Windows, the
system's `SysWOW64` and `System32` directories supply target runtime DLLs.
Use `-lmsvcrt` for C runtime functions such as `puts`; termination then
calls the CRT's `exit`, flushing buffered output. Libc-free programs
terminate through `kernel32.dll`'s `ExitProcess`.

PE output contains loader import metadata and HIGHLOW base relocations
for absolute pointers, enabling rebasing and ASLR. Initialized data and
BSS from ordinary COFF sections are supported. The current object model
combines code and writable data in a read/write/execute image section;
the relocation section is read-only. Startup calls `main` with
`argc == 0` and `argv == NULL`. TLS, static constructors, COMDAT, weak
externals, linker directives, and general MSVC linker-option compatibility
are not supported and produce explicit errors. This mode supports i386
only. `--linker=<path>`
and `--windows-linker=<kind>` are rejected; there is no external fallback.

For Apple Silicon macOS arm64 executables, use the `armv8a-macos` triple
(`--format` is then implied as `macho`):

```sh
rcc --target=armv8a-macos --out=main.o main.c
rcc --link --target=armv8a-macos --out=app \
    main.o -lSystem
```

This mode emits 64-bit arm64 Mach-O objects and internally links
executable images with dyld import/binding metadata and an ad-hoc code signature.
`-lSystem` resolves real system functions such as `puts` through the SDK's
text-based stubs; the resulting executable uses libc's `exit` for termination.
Custom arm64 dylibs and ordinary static archives are supported, including
transitive archive dependencies, imported functions, initialized data, and BSS.
Objects accept `__text`, writable `__data`, and zero-fill `__bss`; other
nonempty sections, TLS, constructor processing, relocations stored in initialized
data, and weak/coalesced-definition selection are unsupported.
Library lookup accepts `-L<dir>`, `-l<name>`, and explicit dylib/archive paths.
Startup calls `main` without command-line arguments. Libc-free programs
use direct syscall termination. Unsupported relocations, linker scripts,
and unresolved symbols fail explicitly.
`--linker=<path>` is unsupported; hosted macOS mode targets Apple Silicon
only.

The `examples/example1/` directory contains `hello.c` and a Makefile with native
`macos`, `linux`, and `windows` build targets. `make -C examples/example1` builds
the target matching the host OS; use `make -C examples/example1 macos`, `linux`,
or `windows` explicitly. Hosted links use rcc's internal linker and do not
require an external system linker.

## Link mode

```
rcc --link [--target=<name>[-<host>]] --format=<target-native> [--gc-sections] [--base=<word-addr>] [--out=file] file1.o [file2.o ...]
```

| Switch | Meaning |
| --- | --- |
| `--link` | Switches from compile mode to link mode. |
| `--target=<name>[-<host>]` | Which target's linker to use. Default: `pdp7`. Every target listed above supports `--link`; none is limited to single-file whole-program compiles. A hosted triple (`i386-linux`, `i386-windows`, `armv8a-macos`) links internally instead - see "Hosted executables" above; `--format` is then implied and `--gc-sections` doesn't apply. |
| `--format=<fmt>` | Final linked output format. Every target except `pdp7`/`8086`/`8088`/`saturn`/`tms320c6713` supports the same uniform set: `flat` (just the final bytes, no container at all), `coff`, `elf`, `aout`. `pdp7` supports its own historical set instead - `a7out`, `list`, `ptr`, `rim`, `coff`, `aout`; `8086`/`8088` support `raw` (a real, directly-bootable 512-byte boot sector) in place of `flat`, plus `coff`, `elf`, `aout`; `saturn` supports only `flat`, `coff` (no `elf`/`aout`). `tms320c6713` has no relocatable object format at all (a first pass - see `docs/platforms.md`), so `--link` isn't supported for it. |
| `--out=file` | Where to write the linked result. Omit it to write to stdout. |
| `--gc-sections` | Drops any input object's own function/global that's never reached from the entry point's own call graph (a real `extern` reference used purely to take a function's address, not to call it, still counts as reached), instead of linking every object in full. Only valid with a non-hosted `--target` (e.g. `pdp7`): each object's own cross-file-visible names (`g_`/`p_`/`fn_`, pdp7's own `G_`/`P_`/`FN_`) mark where one function/global ends and the next begins, so dropping one never removes part of another still in use - an object whose own backend gives no way to tell a same-object address reference's real target apart from any other symbol in it is linked in full regardless, the same as without this flag. |
| `--base=<word-addr>` | Links the given objects to run starting at this word address instead of the target's own ordinary default (`pdp7`: 4096, the start of the real machine's swappable "user" region). Only supported for `--target=pdp7` right now. Lets kernel-resident code (e.g. a boot-time entry sequence, or anything that must stay outside whatever a swappable region's own transfers might overwrite) link starting near word address 0, matching where the real kernel's own low-memory-resident code lives - the entry sequence itself (`jms FN_main; hlt` plus the shared runtime) is unchanged, so this moves *where* a program runs, not *what* runs first. Avoid literal `--base=0` for anything that makes real function calls: this runtime's own `__addr` scratch cell (used for every computed pointer dereference) sits at relative offset 9 from the entry, landing exactly on the real PDP-7's own hardware autoindex register 9 at base 0 (any *indirect* reference to word address 8 or 9 auto-increments it first - see `../../UnixV0-C/src-c/sys/s4.c`'s own header) - confirmed directly to hang real code. A small nonzero base (e.g. 16, clear of both that hazard and whatever a swappable region above it needs) is both safe and matches the real kernel's own layout, which reserves roughly that much low memory for its interrupt vector table anyway. |

Every other flag (`--optimize`, `--dump-ir`, `--dump-asm`) applies only
to compile mode.
