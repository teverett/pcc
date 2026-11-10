# Internal Linux hosted linking

The hosted Linux target emits ELF32/i386 executables directly. It never invokes
a system linker. Compile with `--target=i386-linux`; link the resulting
objects with the same `--target` and `--link`.

`-lc` selects the actual i386 C library. Install its development files (for
example Debian's `libc6-dev:i386`) or provide their directories with `-L`.
Explicit search directories precede the usual i386 system library directories.
There is no built-in libc substitute or fallback.

The linker accepts i386 ELF shared libraries with dynamic-symbol section tables
and GNU library scripts selected by `-l` or supplied as `.so` inputs. The script
subset consists of `INPUT`, `GROUP`, nested `AS_NEEDED`, and
`OUTPUT_FORMAT(elf32-i386)`, with comments, quoted filenames, and comma-separated
inputs. Scripts are bounded to 64 KiB, eight nesting/include levels, and 128
inputs; cycles and unsupported directives are errors. `GROUP` requests a fixed
point across archive members. Archives are extracted lazily; malformed members
are diagnosed. Unused `AS_NEEDED` shared libraries are omitted.
Selected static objects use pcc's single executable `.text` plus `.bss` model;
initialized data, additional allocated sections, and non-text relocations are
explicitly unsupported. This restriction does not apply to shared libraries,
whose segments, data, dependencies, and relocations are handled by Linux's
dynamic loader.

The current object model interleaves writable globals with executable bytes,
so the executable load segment is read/write/execute. The stack is
non-executable.

Shared-library SONAMEs become `DT_NEEDED` entries. Default-visible global/weak
function and GNU IFUNC imports use i386 PLT/GOT `R_386_JMP_SLOT` relocations.
Untyped exports are accepted only for direct PC-relative function references.
Non-default compatibility symbol versions are excluded; the chosen default
versions become GNU version requirements in the executable. Imported data, TLS,
and other unsupported symbol kinds are diagnosed rather than approximated.

When libc is linked, startup passes `main`, `argc`, `argv`, and the dynamic
loader's finalizer to the real `__libc_start_main`. libc initializes the process,
calls `main`, flushes buffered stdio, runs library finalizers, and preserves the
return status. Executables without libc use a minimal direct syscall exit.
The dynamic interpreter is `/lib/ld-linux.so.2`; runtime libraries must be
installed there or found by the Linux loader's normal search mechanism.

Run `tests/run_hosted_link_tests.sh` on an i386-compatible Linux kernel with
the i386 loader and libc development files installed. GNU `as`, `ar`, and `ld`
are used only to construct independent test libraries, not by pcc.
`PCC_LINUX_LIBRARY_PATHS` is a colon-separated list of cross-library directories.
On another host, `PCC_HOSTED_LINUX_PREPARE_ONLY=1`,
`PCC_HOSTED_TEST_LD`, `PCC_HOSTED_TEST_AS`, and `PCC_HOSTED_TEST_AR` prepare cross-target fixtures and
run structural tests without claiming runtime success; execute the prepared
programs separately on Linux.
