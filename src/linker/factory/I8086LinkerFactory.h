#pragma once

#include <memory>

#include "linker/Linker.h"

namespace rcc {

// Builds a GenericLinker shared by the 8086/8088 backend. Plain byte-addressed little-endian
// real-mode x86, so relocations patch via the shared RelocKinds with no local patch function.
std::unique_ptr<TargetLinker> makeI8086Linker();

// Same backend, same instruction encoding, but a real MS-DOS `.com` program's own
// entry/exit convention and load address instead of a bare flat binary's - see
// I8086LinkerFactory.cpp's own `kDosEntryAsm` comment.
std::unique_ptr<TargetLinker> makeI8086DosLinker();

}  // namespace rcc
