#pragma once

#include <cstdint>
#include <string>

#include "fileformat/coff/PEImage.h"

namespace rcc {

// Writes the final linked i386 Windows executable (a real, loadable PE32) for the hosted PE
// linker (see linker/WindowsLinuxLinker.h) - the "target file format" half of that linker, as
// HostedPeReader.h is its "object/library file format" half.

// Where this program's image base-loads; linker/WindowsLinuxLinker.cpp's mergePeObjects() lays out
// `image` against `kPeBase + 0x1000`.
constexpr uint32_t kPeBase = 0x00400000;

// Serializes `image` into a complete PE32 executable: a `.text` section plus, when
// `image.hasWindowsImports`/`image.baseRelocations` call for them, an import table/IAT and a
// `.reloc` base-relocation section.
std::string makePe(const PEImage &image);

}  // namespace rcc
