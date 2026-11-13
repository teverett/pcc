#include "ArmMLinkerFactory.h"

#include "codegen/armm/ArmMAssembler.h"
#include "linker/GenericLinker.h"

namespace rcc {

namespace {

uint16_t get16(const std::vector<uint8_t> &image, size_t idx) {
    return static_cast<uint16_t>(static_cast<uint8_t>(image[idx]) |
                                  (static_cast<uint16_t>(static_cast<uint8_t>(image[idx + 1])) << 8));
}
void put16(std::vector<uint8_t> &image, size_t idx, uint16_t v) {
    image[idx] = static_cast<uint8_t>(v & 0xFF);
    image[idx + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
}

// Reassembles MOVW/MOVT's imm16 (= imm4:i:imm3:imm8) from the two encoded halfwords.
uint32_t extractImm16(uint16_t h1, uint16_t h2) {
    uint32_t imm4 = h1 & 0xFu;
    uint32_t i = (h1 >> 10) & 1u;
    uint32_t imm3 = (h2 >> 12) & 0x7u;
    uint32_t imm8 = h2 & 0xFFu;
    return (imm4 << 12) | (i << 11) | (imm3 << 8) | imm8;
}
// Inverse of extractImm16: writes imm16's sub-fields into h1/h2, leaving other bits untouched.
void injectImm16(uint16_t &h1, uint16_t &h2, uint32_t imm16) {
    uint32_t imm4 = (imm16 >> 12) & 0xFu;
    uint32_t i = (imm16 >> 11) & 1u;
    uint32_t imm3 = (imm16 >> 8) & 0x7u;
    uint32_t imm8 = imm16 & 0xFFu;
    h1 = static_cast<uint16_t>((h1 & ~0x040Fu) | (i << 10) | imm4);
    h2 = static_cast<uint16_t>((h2 & ~0x70FFu) | (imm3 << 12) | imm8);
}

void patchArmMMovWLo(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    uint16_t h1 = get16(image, siteOffset);
    uint16_t h2 = get16(image, siteOffset + 2);
    uint32_t imm16 = (extractImm16(h1, h2) + (value & 0xFFFFu)) & 0xFFFFu;
    injectImm16(h1, h2, imm16);
    put16(image, siteOffset, h1);
    put16(image, siteOffset + 2, h2);
}

// Needs the carry out of the low half's addition, read back from the already-patched MOVW site,
// always exactly 4 bytes earlier.
void patchArmMMovTHi(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    uint16_t lowH1 = get16(image, siteOffset - 4);
    uint16_t lowH2 = get16(image, siteOffset - 2);
    uint32_t lowImm16 = extractImm16(lowH1, lowH2);
    uint32_t carry = (lowImm16 + (value & 0xFFFFu)) > 0xFFFFu ? 1u : 0u;

    uint16_t h1 = get16(image, siteOffset);
    uint16_t h2 = get16(image, siteOffset + 2);
    uint32_t imm16 = (extractImm16(h1, h2) + ((value >> 16) & 0xFFFFu) + carry) & 0xFFFFu;
    injectImm16(h1, h2, imm16);
    put16(image, siteOffset, h1);
    put16(image, siteOffset + 2, h2);
}

// `BL`/wide-`B`'s 32-bit PC-relative field is always zero going in, so it's recomputed from
// scratch (value - (siteFinalAddr + 4), halved, since the offset is always even) and scattered
// across both halfwords via the real S/I1/I2/J1/J2 encoding.
void patchArmMBranch(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t siteFinalAddr) {
    uint32_t pcRelBase = siteFinalAddr + 4;
    int32_t off = static_cast<int32_t>(value) - static_cast<int32_t>(pcRelBase);
    int32_t u = off >> 1;  // off is always even
    uint32_t s = (static_cast<uint32_t>(u) >> 23) & 1u;
    uint32_t i1 = (static_cast<uint32_t>(u) >> 22) & 1u;
    uint32_t i2 = (static_cast<uint32_t>(u) >> 21) & 1u;
    uint32_t imm10 = (static_cast<uint32_t>(u) >> 11) & 0x3FFu;
    uint32_t imm11 = static_cast<uint32_t>(u) & 0x7FFu;
    uint32_t j1 = (i1 ^ s) ^ 1u;
    uint32_t j2 = (i2 ^ s) ^ 1u;

    uint16_t h1 = get16(image, siteOffset);
    uint16_t h2 = get16(image, siteOffset + 2);
    h1 = static_cast<uint16_t>((h1 & ~0x07FFu) | (s << 10) | imm10);
    h2 = static_cast<uint16_t>((h2 & ~0x2FFFu) | (j1 << 13) | (j2 << 11) | imm11);
    put16(image, siteOffset, h1);
    put16(image, siteOffset + 2, h2);
}

// 16 << 16 == 0x100000, the initial stack pointer, split into movw/movt halves.
constexpr const char *kEntryAsm =
    "org 0\n"
    "_start:\n"
    "    movw sp,0\n"
    "    movt sp,16\n"
    "    bl fn_main\n"
    "halt:\n"
    "    b halt\n";

}  // namespace

std::unique_ptr<TargetLinker> makeArmMLinker() {
    GenericLinkerConfig config;
    config.baseAddress = kArmMBaseAddress;
    config.machineType = kArmMCoffMachine;
    config.entryObject = assembleArmMObject(kEntryAsm);
    config.relocPatchers[kArmMRelocMovWLo] = patchArmMMovWLo;
    config.relocPatchers[kArmMRelocMovTHi] = patchArmMMovTHi;
    config.relocPatchers[kArmMRelocBranch] = patchArmMBranch;
    config.outputFormats = {"raw", "coff", "elf", "aout"};
    return std::make_unique<GenericLinker>(std::move(config));
}

}  // namespace rcc
