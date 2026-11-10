#pragma once

#include <memory>

#include "linker/Linker.h"

namespace rcc {

// Builds a GenericLinker for PIC16, with masked local patchers for the
// 11-bit GOTO/CALL field and the 7-bit data field, and a synthesized
// `_start` entry object assembled like any other object.
std::unique_ptr<TargetLinker> makePIC16Linker();

}  // namespace rcc
