#pragma once

#include <string>

#include "fileformat/elf/ELFImage.h"

namespace rcc {

// Reads real i386 ELF input for the hosted linker (see linker/WindowsLinuxLinker.h) - unlike
// ElfFormat.h/.cpp's reader/writer pair, which only ever round-trips this project's own simplified
// freestanding layout.

// Parses `bytes` (a real ET_DYN shared library, e.g. libc.so.6) into its exported symbols and
// SONAME. `name` is only used as a fallback name and in error messages.
DynamicLibrary readElfDynamicLibrary(const std::string &bytes, const std::string &name);

// True if `bytes` looks like a real ET_DYN i386 ELF shared library.
bool isElfDynamicImage(const std::string &bytes);

// Throws std::runtime_error unless `bytes` is a little-endian i386 ET_REL object with the shape
// the hosted ELF linker supports (one allocated `.text`, at most one allocated `.bss`, relocations
// only against `.text`).
void validateHostedElfObject(const std::string &bytes);

}  // namespace rcc
