#include "M65C02LinkerFactory.h"

#include <cstdint>
#include <stdexcept>

#include "linker/GenericLinker.h"
#include "codegen/m65c02/M65C02Assembler.h"

namespace pcc {

namespace {
constexpr const char *kEntryAsm =
    "org 0\n"
    "ptr = 0\n"
    "ac = 2\n"
    "bc = 4\n"
    "cx = 6\n"
    "dx = 8\n"
    "_start:\n"
    "    jsr fn_main\n"
    "    lda ac\n"
    "halt:\n"
    "    jmp halt\n";

// One-byte additive patch on the low half of a 16-bit resolved
// value - see M65C02Assembler.h's own `kM65C02RelocAbsLo8` comment
// on why this is a local function here rather than a shared
// GenericLinker::RelocKinds entry.
void patchAbsLo8(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    image[siteOffset] = static_cast<uint8_t>((image[siteOffset] + (value & 0xFF)) & 0xFF);
}
// The high half.
void patchAbsHi8(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    image[siteOffset] = static_cast<uint8_t>((image[siteOffset] + ((value >> 8) & 0xFF)) & 0xFF);
}
}  // namespace

std::unique_ptr<TargetLinker> makeM65C02Linker() {
    GenericLinkerConfig config;
    config.baseAddress = kM65C02BaseAddress;
    config.machineType = kM65C02CoffMachine;
    config.entryObject = assembleM65C02Object(kEntryAsm);
    config.relocPatchers[kM65C02RelocAbs16] = RelocKinds::patchAbs16;
    config.relocPatchers[kM65C02RelocAbsLo8] = patchAbsLo8;
    config.relocPatchers[kM65C02RelocAbsHi8] = patchAbsHi8;
    config.outputFormats = {"flat", "coff", "elf", "aout"};
    return std::make_unique<GenericLinker>(std::move(config));
}

}  // namespace pcc
