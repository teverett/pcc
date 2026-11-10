#pragma once

#include <memory>

#include "linker/Linker.h"

namespace rcc {

std::unique_ptr<TargetLinker> makeSaturnLinker();

}  // namespace rcc
