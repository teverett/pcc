#include "Arm64LinkerFactory.h"

#include "codegen/arm64/Arm64Assembler.h"
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

// `b`/`bl` imm26 field: pure PC-relative offset from this instruction's own address, with no
// "+8"/"+4" pipeline quirk (unlike classic ARM A32).
void patchA64Rel26(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t siteFinalAddr) {
    int64_t offset = static_cast<int64_t>(value) - static_cast<int64_t>(siteFinalAddr);
    uint32_t imm26 = static_cast<uint32_t>(offset >> 2) & 0x3FFFFFFu;
    uint32_t word = get32(image, siteOffset);
    word = (word & ~0x3FFFFFFu) | imm26;
    put32(image, siteOffset, word);
}

// `movz`'s 16-bit immediate (bits [20:5]), additive: a truncated mod-65536 addition is always
// carry-correct on its own.
void patchA64MovZLo(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    uint32_t word = get32(image, siteOffset);
    uint32_t imm16 = (word >> 5) & 0xFFFFu;
    imm16 = (imm16 + (value & 0xFFFFu)) & 0xFFFFu;
    word = (word & ~(0xFFFFu << 5)) | (imm16 << 5);
    put32(image, siteOffset, word);
}

// `movk`'s high 16-bit immediate: needs the carry out of the low half's addition, read back from
// the already-patched `movz` site, always exactly 4 bytes earlier (movz always precedes movk, and
// relocations patch in emission order).
void patchA64MovKHi(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    uint32_t lowWord = get32(image, siteOffset - 4);
    uint32_t lowImm = (lowWord >> 5) & 0xFFFFu;
    uint32_t carry = (lowImm + (value & 0xFFFFu)) > 0xFFFFu ? 1u : 0u;

    uint32_t word = get32(image, siteOffset);
    uint32_t imm16 = (word >> 5) & 0xFFFFu;
    imm16 = (imm16 + ((value >> 16) & 0xFFFFu) + carry) & 0xFFFFu;
    word = (word & ~(0xFFFFu << 5)) | (imm16 << 5);
    put32(image, siteOffset, word);
}

// 17408 << 16 == 0x44000000, the initial stack pointer, split into movz/movk halves.
constexpr const char *kEntryAsm =
    "org 0\n"
    "_start:\n"
    "    movz x9,0\n"
    "    movk x9,17408\n"
    "    add sp,x9,0\n"
    "    bl fn_main\n"
    "halt:\n"
    "    b halt\n";

}  // namespace

std::unique_ptr<TargetLinker> makeArm64Linker() {
    GenericLinkerConfig config;
    config.baseAddress = static_cast<uint32_t>(kArm64BaseAddress);
    config.machineType = kArm64CoffMachine;
    config.entryObject = assembleArm64Object(kEntryAsm);
    config.relocPatchers[kArm64RelocRel26] = patchA64Rel26;
    config.relocPatchers[kArm64RelocMovZLo] = patchA64MovZLo;
    config.relocPatchers[kArm64RelocMovKHi] = patchA64MovKHi;
    config.outputFormats = {"raw", "coff", "elf", "aout"};
    return std::make_unique<GenericLinker>(std::move(config));
}

}  // namespace rcc
