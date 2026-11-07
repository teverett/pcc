#include "PowerPCLinkerFactory.h"

#include "codegen/powerpc/PowerPCAssembler.h"
#include "linker/GenericLinker.h"

namespace pcc {

namespace {

// PowerPC instruction words are always big-endian.
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

// `b`/`bl`: opcode(6)=18 LI(24) AA(1)=0 LK(1). PC-relative from this instruction's own address, no
// ARM-style pipeline offset.
void patchPowerPCBranch24(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t siteFinalAddr) {
    int32_t rel = static_cast<int32_t>(value) - static_cast<int32_t>(siteFinalAddr);
    uint32_t word = get32BE(image, siteOffset);
    word = (word & ~0x03FFFFFCu) | (static_cast<uint32_t>(rel) & 0x03FFFFFCu);
    put32BE(image, siteOffset, word);
}

// Upper half of a `lis`/`ori` address-load pair. The linker's correction to each 16-bit half can
// carry from low into high, so this reads the paired Lo site (siteOffset+4, always the next
// instruction) for its pre-patch value; relocations are patched in code order, so Hi is applied
// before Lo.
void patchPowerPCHi(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    uint32_t loWord = get32BE(image, siteOffset + 4);
    uint32_t loImm = loWord & 0xFFFFu;
    uint32_t carry = (loImm + (value & 0xFFFFu)) > 0xFFFFu ? 1u : 0u;

    uint32_t word = get32BE(image, siteOffset);
    uint32_t imm16 = word & 0xFFFFu;
    imm16 = (imm16 + ((value >> 16) & 0xFFFFu) + carry) & 0xFFFFu;
    word = (word & ~0xFFFFu) | imm16;
    put32BE(image, siteOffset, word);
}

// Lower half of the pair above; must patch strictly after Hi reads this site's pre-patch value.
void patchPowerPCLo(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    uint32_t word = get32BE(image, siteOffset);
    uint32_t imm16 = word & 0xFFFFu;
    imm16 = (imm16 + (value & 0xFFFFu)) & 0xFFFFu;
    word = (word & ~0xFFFFu) | imm16;
    put32BE(image, siteOffset, word);
}

// Plain contiguous 32-bit field; value adds straight onto the pre-patch word.
void patchPowerPCAbs32(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    put32BE(image, siteOffset, get32BE(image, siteOffset) + value);
}

// 32/0 are kInitialSP (0x00200000) split into upper/lower 16 bits.
constexpr const char *kEntryAsm =
    "org 0\n"
    "_start:\n"
    "    lis sp,32\n"
    "    ori sp,sp,0\n"
    "    bl fn_main\n"
    "halt:\n"
    "    b halt\n";

}  // namespace

std::unique_ptr<TargetLinker> makePowerPCLinker() {
    GenericLinkerConfig config;
    config.baseAddress = kPowerPCBaseAddress;
    config.machineType = kPowerPCCoffMachine;
    config.entryObject = assemblePowerPCObject(kEntryAsm);
    config.relocPatchers[kPowerPCRelocHi] = patchPowerPCHi;
    config.relocPatchers[kPowerPCRelocLo] = patchPowerPCLo;
    config.relocPatchers[kPowerPCRelocBranch] = patchPowerPCBranch24;
    config.relocPatchers[kPowerPCRelocAbs32] = patchPowerPCAbs32;
    config.outputFormats = {"flat", "coff", "elf", "aout"};
    return std::make_unique<GenericLinker>(std::move(config));
}

}  // namespace pcc
