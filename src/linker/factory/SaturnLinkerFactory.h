#pragma once

#include <memory>

#include "linker/Linker.h"

namespace pcc {

std::unique_ptr<TargetLinker> makeSaturnLinker();

}  // namespace pcc
