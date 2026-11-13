#include "Pdp11LinkerFactory.h"

#include "linker/GenericLinker.h"
#include "codegen/pdp11/Pdp11Assembler.h"

namespace rcc {

namespace {
// Mode-067 PC-relative extension word: the displacement is relative to
// the extension word's own address, with no +2 adjustment (unlike the
// shared RelocKinds::patchRel16).
void patchRel16NoAdj(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t siteFinalAddr) {
    int32_t rel = static_cast<int32_t>(value) - static_cast<int32_t>(siteFinalAddr);
    image[siteOffset] = static_cast<uint8_t>(rel & 0xFF);
    image[siteOffset + 1] = static_cast<uint8_t>((rel >> 8) & 0xFF);
}

// 0xE000 must match Pdp11Codegen.cpp's private kInitialSp.
constexpr const char *kEntryAsm =
    "org 0\n"
    "_start:\n"
    "    mov 57344,sp\n"
    "    jsr fn_main\n"
    "halt:\n"
    "    halt\n"
    "    br halt\n";
}  // namespace

std::unique_ptr<TargetLinker> makePdp11Linker() {
    GenericLinkerConfig config;
    config.baseAddress = kPdp11BaseAddress;
    config.machineType = kPdp11CoffMachine;
    config.entryObject = assemblePdp11Object(kEntryAsm);
    config.relocPatchers[kPdp11RelocAbs16] = RelocKinds::patchAbs16;
    config.relocPatchers[kPdp11RelocRel16] = patchRel16NoAdj;
    config.outputFormats = {"raw", "coff", "elf", "aout"};
    return std::make_unique<GenericLinker>(std::move(config));
}

}  // namespace rcc
