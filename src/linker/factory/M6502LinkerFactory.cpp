#include "M6502LinkerFactory.h"

#include "linker/GenericLinker.h"
#include "codegen/m6502/M6502Assembler.h"

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

// The low/high-byte-only patch kinds this dialect needs beyond
// GenericLinker's own shared `RelocKinds` (see
// ../codegen/m6502/M6502Assembler.h's own `kM6502RelocAbs16Lo`/`Hi`
// comment) - kept local to this file rather than added to
// GenericLinker.h's own shared namespace, since no other
// GenericLinker-based target needs this exact "patch only one byte
// of a 16-bit value" shape (see that header's own comment on when a
// target needs its own dedicated kind instead of a shared one).
void patchAbs16Lo(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    image[siteOffset] = static_cast<uint8_t>(image[siteOffset] + (value & 0xFF));
}
void patchAbs16Hi(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    image[siteOffset] = static_cast<uint8_t>(image[siteOffset] + ((value >> 8) & 0xFF));
}
}  // namespace

std::unique_ptr<TargetLinker> makeM6502Linker() {
    GenericLinkerConfig config;
    config.baseAddress = kM6502BaseAddress;
    config.machineType = kM6502CoffMachine;
    config.entryObject = assembleM6502Object(kEntryAsm);
    config.relocPatchers[kM6502RelocAbs16] = RelocKinds::patchAbs16;
    config.relocPatchers[kM6502RelocAbs16Lo] = patchAbs16Lo;
    config.relocPatchers[kM6502RelocAbs16Hi] = patchAbs16Hi;
    config.outputFormats = {"flat", "coff", "elf", "aout"};
    return std::make_unique<GenericLinker>(std::move(config));
}

}  // namespace pcc
