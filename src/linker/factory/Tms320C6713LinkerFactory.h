#pragma once

#include <memory>

#include "linker/Linker.h"

namespace pcc {

// This target's first pass only implements whole-program --format=raw (see
// Tms320C6713Codegen.h's own header comment) - there's no relocatable object format a user can
// ever produce for it, so there's nothing a real --link could ever receive. makeTms320C6713Linker()
// still exists so --target=tms320c6713 is wired into main.cpp's makeLinker() the same way every
// other target is; its GenericLinker reports no outputFormats(), so --link --target=tms320c6713
// fails with a clear "unknown --format" message instead of silently doing nothing.
std::unique_ptr<TargetLinker> makeTms320C6713Linker();

}  // namespace pcc
