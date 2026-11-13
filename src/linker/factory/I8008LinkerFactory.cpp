#include "I8008LinkerFactory.h"

#include "linker/GenericLinker.h"
#include "codegen/i8008/I8008Assembler.h"

namespace rcc {

namespace {
// ac/bc/cx/dx/ptr/sp are the shared pseudo-registers at kPseudoBase (15360/0x3c00) + 0/2/4/6/8/10,
// spelled as literal decimal constants since this text has no #include mechanism of its own.
constexpr const char *kEntryAsm =
    "org 0\n"
    "ac = 15360\n"
    "bc = 15362\n"
    "cx = 15364\n"
    "dx = 15366\n"
    "ptr = 15368\n"
    "sp = 15370\n"
    "_start:\n"
    "    mvihi h, sp\n"
    "    mvi l, sp\n"
    "    mvi a, 15372\n"
    "    mov m, a\n"
    "    mvihi h, sp+1\n"
    "    mvi l, sp+1\n"
    "    mvihi a, 15372\n"
    "    mov m, a\n"
    "    cal fn_main\n"
    "halt:\n"
    "    jmp halt\n";

// These two always patch a single mvihi/mvi pair; the Hi site's offset is always exactly 2 less
// than its Lo partner's, and Hi is patched before Lo, so the carry out of the (still unpatched)
// low-byte addition can be computed by reading the Lo site directly.
void patchAbs16Lo(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    image[siteOffset] = static_cast<uint8_t>(image[siteOffset] + (value & 0xFF));
}
void patchAbs16Hi(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    uint32_t loByte = image[siteOffset + 2];
    uint32_t carry = (loByte + (value & 0xFF)) > 0xFF ? 1 : 0;
    image[siteOffset] = static_cast<uint8_t>(image[siteOffset] + ((value >> 8) & 0xFF) + carry);
}
}  // namespace

std::unique_ptr<TargetLinker> makeI8008Linker() {
    GenericLinkerConfig config;
    config.baseAddress = kI8008BaseAddress;
    config.machineType = kI8008CoffMachine;
    config.entryObject = assembleI8008Object(kEntryAsm);
    config.relocPatchers[kI8008RelocAbs16] = RelocKinds::patchAbs16;
    config.relocPatchers[kI8008RelocAbs16Lo] = patchAbs16Lo;
    config.relocPatchers[kI8008RelocAbs16Hi] = patchAbs16Hi;
    config.outputFormats = {"raw", "coff", "elf", "aout"};
    return std::make_unique<GenericLinker>(std::move(config));
}

}  // namespace rcc
