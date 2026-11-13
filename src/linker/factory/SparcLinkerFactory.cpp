#include "SparcLinkerFactory.h"

#include "codegen/sparc/SparcAssembler.h"
#include "linker/GenericLinker.h"

namespace rcc {

namespace {

// SPARC instruction words are always big-endian.
uint32_t get32BE(const std::vector<uint8_t> &image, size_t idx) {
    return (static_cast<uint32_t>(image[idx]) << 24) | (static_cast<uint32_t>(image[idx + 1]) << 16) |
           (static_cast<uint32_t>(image[idx + 2]) << 8) | static_cast<uint32_t>(image[idx + 3]);
}
void put32BE(std::vector<uint8_t> &image, size_t idx, uint32_t v) {
    image[idx] = static_cast<uint8_t>((v >> 24) & 0xFF);
    image[idx + 1] = static_cast<uint8_t>((v >> 16) & 0xFF);
    image[idx + 2] = static_cast<uint8_t>((v >> 8) & 0xFF);
    image[idx + 3] = static_cast<uint8_t>(v & 0xFF);
}

// `call` format 1: op(2)=01 disp30(30). PC-relative from this instruction's own address, no
// ARM-style pipeline offset.
void patchSparcCall30(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t siteFinalAddr) {
    int32_t rel = static_cast<int32_t>(value) - static_cast<int32_t>(siteFinalAddr);
    uint32_t disp30 = (static_cast<uint32_t>(rel) >> 2) & 0x3FFFFFFFu;
    uint32_t word = get32BE(image, siteOffset);
    word = (word & 0xC0000000u) | disp30;
    put32BE(image, siteOffset, word);
}

// Upper 22 bits of a `sethi`/`or` address-load pair. The linker's correction to each field can
// carry from the 10-bit Lo field into Hi, so this reads the paired Lo site (siteOffset+4, always
// the next instruction) for its pre-patch value; relocations are patched in code order, so Hi is
// applied before Lo.
void patchSparcHi(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    uint32_t loWord = get32BE(image, siteOffset + 4);
    uint32_t loField = loWord & 0x3FFu;
    uint32_t carry = (loField + (value & 0x3FFu)) > 0x3FFu ? 1u : 0u;

    uint32_t word = get32BE(image, siteOffset);
    uint32_t imm22 = word & 0x3FFFFFu;
    imm22 = (imm22 + ((value >> 10) & 0x3FFFFFu) + carry) & 0x3FFFFFu;
    word = (word & ~0x3FFFFFu) | imm22;
    put32BE(image, siteOffset, word);
}

// Lower 10 bits of the pair above; must patch strictly after Hi reads this site's pre-patch value.
void patchSparcLo(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    uint32_t word = get32BE(image, siteOffset);
    uint32_t imm10 = word & 0x3FFu;
    imm10 = (imm10 + (value & 0x3FFu)) & 0x3FFu;
    word = (word & ~0x3FFu) | imm10;
    put32BE(image, siteOffset, word);
}

// Plain contiguous 32-bit field; value adds straight onto the pre-patch word.
void patchSparcAbs32(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    put32BE(image, siteOffset, get32BE(image, siteOffset) + value);
}

// 1024/0 are kInitialSP (0x00100000) split into upper-22/lower-10 bits.
constexpr const char *kEntryAsm =
    "org 0\n"
    "_start:\n"
    "    sethi 1024,sp\n"
    "    or sp,0,sp\n"
    "    call fn_main\n"
    "halt:\n"
    "    ba halt\n";

}  // namespace

std::unique_ptr<TargetLinker> makeSparcLinker() {
    GenericLinkerConfig config;
    config.baseAddress = kSparcBaseAddress;
    config.machineType = kSparcCoffMachine;
    config.entryObject = assembleSparcObject(kEntryAsm);
    config.relocPatchers[kSparcRelocHi] = patchSparcHi;
    config.relocPatchers[kSparcRelocLo] = patchSparcLo;
    config.relocPatchers[kSparcRelocCall] = patchSparcCall30;
    config.relocPatchers[kSparcRelocAbs32] = patchSparcAbs32;
    config.outputFormats = {"raw", "coff", "elf", "aout"};
    return std::make_unique<GenericLinker>(std::move(config));
}

}  // namespace rcc
