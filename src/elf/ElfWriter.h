#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace pcc {

// A minimal, from-scratch 32-bit little-endian ELF executable writer,
// the 8086 target's third peer of CoffWriter.h/BinWriter.h (shared
// the same way CoffWriter.h is - by codegen/x86_8086/I8086Assembler.cpp's
// own in-process assembler, for a single self-contained --format=elf
// compile, and by linker/format/ElfWriter.h's thin FormatWriter
// wrapper, for `--link --format=elf`). Writes exactly one `PT_LOAD`
// segment covering `bytes` as-is, loaded and entered at `base` - no
// section header table, no symbol table, no relocations: this
// project's own single-flat-blob model (no .text/.data split
// anywhere - see CoffWriter.h's analogous note) has nothing for
// those to describe, the same reasoning AoutWriter.cpp's OMAGIC
// comment already gives for `aout`.
//
// `e_machine` is EM_386 (3) - there's no registered ELF machine ID
// for 16-bit real-mode 8086 at all, so this is the closest real one,
// the same spirit as CoffWriter.h's own choice of a Machine value
// real tooling at least recognizes as "x86-ish" (that file's header
// comment notes IMAGE_FILE_MACHINE_I386/0x14c makes a real COFF
// reader accept its output). A real ELF loader would still refuse to
// *run* this (it expects 32-bit protected-mode code, not a 16-bit
// real-mode boot sector) - same honestly-incomplete-executable
// status as `--format=aout`, not a claim that this is bootable or
// runnable by anything other than `--format=bin`'s own convention.
std::string writeElfExecutable(uint32_t base, const std::vector<uint8_t> &bytes);

}  // namespace pcc
