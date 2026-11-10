#include "Tms320C6713LinkerFactory.h"

#include <cstdint>

#include "linker/GenericLinker.h"

namespace rcc {

namespace {
// Matches Tms320C6713Codegen.cpp's own kBaseAddress (not otherwise exported - this linker never
// actually lays anything out, see this file's own header comment).
constexpr uint32_t kTms320C6713BaseAddress = 0x00020000u;
// Non-registered machine ID derived from "6713"; never matched against anything real since
// this target has no object format to produce or consume.
constexpr uint16_t kTms320C6713Machine = 0x6713;
}  // namespace

std::unique_ptr<TargetLinker> makeTms320C6713Linker() {
    GenericLinkerConfig config;
    config.baseAddress = kTms320C6713BaseAddress;
    config.machineType = kTms320C6713Machine;
    config.outputFormats = {};  // see this file's own header comment
    return std::make_unique<GenericLinker>(std::move(config));
}

}  // namespace rcc
