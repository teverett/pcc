#include "M6502LinkerFactory.h"

#include "linker/GenericLinker.h"
#include "codegen/m6502/M6502Assembler.h"

namespace rcc {

namespace {
// sp must be seeded to 0x0200 (kFrameStackBase in M6502Codegen.cpp) before
// the first call, matching that file's own _start initialization.
constexpr const char *kEntryAsm =
    "org 0\n"
    "ptr = 0\n"
    "ac = 2\n"
    "bc = 4\n"
    "cx = 6\n"
    "dx = 8\n"
    "fp = 10\n"
    "sp = 12\n"
    "aBase = 14\n"
    "_start:\n"
    "    ldai 0\n"
    "    sta sp\n"
    "    ldai 2\n"
    "    sta sp+1\n"
    "    jsr fn_main\n"
    "    lda ac\n"
    "halt:\n"
    "    jmp halt\n";

// Patches only the low/high byte of a 16-bit value.
void patchAbs16Lo(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    image[siteOffset] = static_cast<uint8_t>(image[siteOffset] + (value & 0xFF));
}
void patchAbs16Hi(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    image[siteOffset] = static_cast<uint8_t>(image[siteOffset] + ((value >> 8) & 0xFF));
}
}  // namespace

std::unique_ptr<TargetLinker> makeM6502Linker(uint32_t baseAddress) {
    GenericLinkerConfig config;
    config.baseAddress = baseAddress;
    config.machineType = kM6502CoffMachine;
    config.entryObject = assembleM6502Object(kEntryAsm);
    config.relocPatchers[kM6502RelocAbs16] = RelocKinds::patchAbs16;
    config.relocPatchers[kM6502RelocAbs16Lo] = patchAbs16Lo;
    config.relocPatchers[kM6502RelocAbs16Hi] = patchAbs16Hi;
    config.outputFormats = {"raw", "coff", "elf", "aout", "dos33", "prodos", "prg"};
    return std::make_unique<GenericLinker>(std::move(config));
}

}  // namespace rcc
