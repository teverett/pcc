#include "ArmLinkerFactory.h"

#include "codegen/arm/ArmAssembler.h"
#include "linker/GenericLinker.h"

namespace rcc {

namespace {

uint32_t get32(const std::vector<uint8_t> &image, size_t idx) {
    return static_cast<uint32_t>(image[idx]) | (static_cast<uint32_t>(image[idx + 1]) << 8) |
           (static_cast<uint32_t>(image[idx + 2]) << 16) | (static_cast<uint32_t>(image[idx + 3]) << 24);
}
void put32(std::vector<uint8_t> &image, size_t idx, uint32_t v) {
    image[idx] = static_cast<uint8_t>(v & 0xFF);
    image[idx + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
    image[idx + 2] = static_cast<uint8_t>((v >> 16) & 0xFF);
    image[idx + 3] = static_cast<uint8_t>((v >> 24) & 0xFF);
}

// `B`/`BL`'s 24-bit word-granularity offset field. The "+8" reproduces A32's architectural "PC
// reads as this instruction's address plus 8" convention.
void patchArmRel24(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t siteFinalAddr) {
    int32_t rel = (static_cast<int32_t>(value) - static_cast<int32_t>(siteFinalAddr + 8)) / 4;
    uint32_t word = get32(image, siteOffset);
    word = (word & ~0x00FFFFFFu) | (static_cast<uint32_t>(rel) & 0x00FFFFFFu);
    put32(image, siteOffset, word);
}

// `MOVW`'s imm4:imm12 split (bits [19:16], [11:0]), additive: a truncated mod-65536 addition is
// always carry-correct on its own.
void patchArmMovWLo(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    uint32_t word = get32(image, siteOffset);
    uint32_t imm16 = (((word >> 16) & 0xFu) << 12) | (word & 0xFFFu);
    imm16 = (imm16 + (value & 0xFFFFu)) & 0xFFFFu;
    word = (word & ~(0xFu << 16) & ~0xFFFu) | (((imm16 >> 12) & 0xFu) << 16) | (imm16 & 0xFFFu);
    put32(image, siteOffset, word);
}

// `MOVT`'s high half: needs the carry out of the low half's addition, read back from the
// already-patched `movw` site, always exactly 4 bytes earlier.
void patchArmMovTHi(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    uint32_t lowWord = get32(image, siteOffset - 4);
    uint32_t lowImm = (((lowWord >> 16) & 0xFu) << 12) | (lowWord & 0xFFFu);
    uint32_t carry = (lowImm + (value & 0xFFFFu)) > 0xFFFFu ? 1u : 0u;

    uint32_t word = get32(image, siteOffset);
    uint32_t imm16 = (((word >> 16) & 0xFu) << 12) | (word & 0xFFFu);
    imm16 = (imm16 + ((value >> 16) & 0xFFFFu) + carry) & 0xFFFFu;
    word = (word & ~(0xFu << 16) & ~0xFFFu) | (((imm16 >> 12) & 0xFu) << 16) | (imm16 & 0xFFFu);
    put32(image, siteOffset, word);
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

std::unique_ptr<TargetLinker> makeArmLinker() {
    GenericLinkerConfig config;
    config.baseAddress = kArmBaseAddress;
    config.machineType = kArmCoffMachine;
    config.entryObject = assembleArmObject(kEntryAsm);
    config.relocPatchers[kArmRelocRel24] = patchArmRel24;
    config.relocPatchers[kArmRelocMovWLo] = patchArmMovWLo;
    config.relocPatchers[kArmRelocMovTHi] = patchArmMovTHi;
    config.outputFormats = {"flat", "coff", "elf", "aout"};
    return std::make_unique<GenericLinker>(std::move(config));
}

}  // namespace rcc
