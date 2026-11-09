#pragma once

#include <cstdint>
#include <string>

#include "fileformat/elf/ELFImage.h"

namespace pcc {

// Writes the final linked i386 Linux executable (a real, loadable ELF - statically linked, or
// dynamically linked against `image.dynamicLibraries`) for the hosted ELF linker (see
// linker/HostedLinker.h) - the "target file format" half of that linker, as HostedElfReader.h is
// its "object/library file format" half.

// Where this program's `.text` loads; linker/HostedLinker.cpp's mergeElfObjects() lays out `image`
// against `kElfBase + 0x1000`.
constexpr uint32_t kElfBase = 0x08048000;

// Serializes `image` into a complete ELF executable (ET_EXEC, PT_INTERP + PT_DYNAMIC added only
// when `image.dynamicLibraries` is non-empty).
std::string makeElf(const ELFImage &image);

}  // namespace pcc
