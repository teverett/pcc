#include "Pdp11LinkerFactory.h"

#include "linker/GenericLinker.h"
#include "codegen/pdp11/Pdp11Assembler.h"

namespace pcc {

namespace {
// This dialect's own PC-relative (mode 067) extension-word formula -
// see Pdp11Assembler.h's own `kPdp11RelocRel16` comment for exactly
// why this differs from GenericLinker.h's own shared
// `RelocKinds::patchRel16` (no +2: the extension word's own address
// *is* the site this displacement is relative to, confirmed directly
// from Pdp11Assembler.cpp's own `emitEAExtension`'s `Mem` case -
// `target - extAddr`, where `extAddr` is that same word's own
// address). Kept local to this file rather than added to the shared
// `RelocKinds` namespace, same precedent as M6502LinkerFactory.cpp's
// own locally-scoped `AbsLo8`/`AbsHi8` - a one-off variant only this
// target needs.
void patchRel16NoAdj(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t siteFinalAddr) {
    int32_t rel = static_cast<int32_t>(value) - static_cast<int32_t>(siteFinalAddr);
    image[siteOffset] = static_cast<uint8_t>(rel & 0xFF);
    image[siteOffset + 1] = static_cast<uint8_t>((rel >> 8) & 0xFF);
}

// `0xE000` is the same initial-stack-pointer value as
// Pdp11Codegen.cpp's own (private) `kInitialSp` - comfortably above
// the loaded program and its data, same reasoning as that file's own
// comment.
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
    config.outputFormats = {"flat", "coff", "elf", "aout"};
    return std::make_unique<GenericLinker>(std::move(config));
}

}  // namespace pcc
