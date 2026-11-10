#pragma once

#include <memory>

#include "linker/Linker.h"

namespace rcc {

// Builds a GenericLinker for the 6502, with three relocation kinds
// (16-bit absolute plus byte-only Lo/Hi halves) and a synthesized
// `_start` entry object assembled like any other object.
std::unique_ptr<TargetLinker> makeM6502Linker();

}  // namespace rcc
