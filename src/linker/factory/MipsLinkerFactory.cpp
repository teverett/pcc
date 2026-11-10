#include "MipsLinkerFactory.h"

#include "codegen/mips/MipsAssembler.h"
#include "linker/GenericLinker.h"

namespace rcc {

namespace {

// Big-endian 32-bit instruction-word access.
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

// Patches ori's 16-bit immediate (the low address half). A truncated
// mod-65536 addition is carry-correct regardless of patch order.
void patchMipsLo(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    uint32_t word = get32BE(image, siteOffset);
    uint32_t imm16 = ((word & 0xFFFFu) + (value & 0xFFFFu)) & 0xFFFFu;
    put32BE(image, siteOffset, (word & 0xFFFF0000u) | imm16);
}

// Patches lui's 16-bit immediate (the high address half), needing the
// carry out of the paired ori site 4 bytes later. Relocations are
// patched in order and lui is always recorded before its ori, so the
// Lo field read here is still its pre-patch value.
void patchMipsHi(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    uint32_t loImm = get32BE(image, siteOffset + 4) & 0xFFFFu;
    uint32_t carry = (loImm + (value & 0xFFFFu)) > 0xFFFFu ? 1u : 0u;
    uint32_t word = get32BE(image, siteOffset);
    uint32_t imm16 = ((word & 0xFFFFu) + ((value >> 16) & 0xFFFFu) + carry) & 0xFFFFu;
    put32BE(image, siteOffset, (word & 0xFFFF0000u) | imm16);
}

// Patches j/jal's 26-bit instr_index field (bits [25:0]). Values are
// always word-aligned, so shifting each operand right by 2 before
// adding is exact, with no dropped-bit carry issue.
void patchMipsJump(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    uint32_t word = get32BE(image, siteOffset);
    uint32_t index = ((word & 0x03FFFFFFu) + (value >> 2)) & 0x03FFFFFFu;
    put32BE(image, siteOffset, (word & 0xFC000000u) | index);
}

// Patches a plain 32-bit dw data word.
void patchMipsAbs32(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    put32BE(image, siteOffset, get32BE(image, siteOffset) + value);
}

// 32784/0 are the hi/lo halves of 0x80100000, matching MipsCodegen.cpp's
// private kInitialSP.
constexpr const char *kEntryAsm =
    "org 0\n"
    "_start:\n"
    "    lui sp,32784\n"
    "    ori sp,sp,0\n"
    "    jal fn_main\n"
    "    nop\n"
    "halt:\n"
    "    beq zero,zero,halt\n"
    "    nop\n";

}  // namespace

std::unique_ptr<TargetLinker> makeMipsLinker() {
    GenericLinkerConfig config;
    config.baseAddress = kMipsBaseAddress;
    config.machineType = kMipsCoffMachine;
    config.entryObject = assembleMipsObject(kEntryAsm);
    config.relocPatchers[kMipsRelocHi] = patchMipsHi;
    config.relocPatchers[kMipsRelocLo] = patchMipsLo;
    config.relocPatchers[kMipsRelocJump] = patchMipsJump;
    config.relocPatchers[kMipsRelocAbs32] = patchMipsAbs32;
    config.outputFormats = {"flat", "coff", "elf", "aout"};
    return std::make_unique<GenericLinker>(std::move(config));
}

}  // namespace rcc
