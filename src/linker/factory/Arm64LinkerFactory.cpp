#include "Arm64LinkerFactory.h"

#include "codegen/arm64/Arm64Assembler.h"
#include "linker/GenericLinker.h"

namespace pcc {

namespace {

// Little-endian 32-bit load/store - every AArch64 instruction word
// here is always little-endian in memory (see Arm64Assembler.cpp's
// own `emit32` comment).
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

// `b`/`bl` - opcode(6)|imm26(26), already baked in by the assembler
// except for imm26 (zero, since this relocation only ever fires for
// a target undefined in this object - see Arm64Assembler.cpp's own
// `b`/`bl` comment on why a same-object branch needs no relocation at
// all). Empirically verified against `llvm-objdump` in this exact
// environment: offset is pure PC-relative from this instruction's own
// address, with no "+8"/"+4" pipeline quirk (unlike classic ARM A32).
void patchA64Rel26(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t siteFinalAddr) {
    int64_t offset = static_cast<int64_t>(value) - static_cast<int64_t>(siteFinalAddr);
    uint32_t imm26 = static_cast<uint32_t>(offset >> 2) & 0x3FFFFFFu;
    uint32_t word = get32(image, siteOffset);
    word = (word & ~0x3FFFFFFu) | imm26;
    put32(image, siteOffset, word);
}

// `movz`'s own 16-bit immediate, contiguous at bits [20:5] - additive
// (the field already holds whatever locally-resolved half the
// assembler baked in - see Arm64Assembler.h's own header comment on
// why every absolute-address reference needs this, not just an
// external one). Low half: a plain truncated mod-65536 addition is
// always carry-correct on its own (see ../../codegen/i8008/
// I8008LinkerFactory.cpp's own identical reasoning for its own Lo/Hi
// split).
void patchA64MovZLo(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    uint32_t word = get32(image, siteOffset);
    uint32_t imm16 = (word >> 5) & 0xFFFFu;
    imm16 = (imm16 + (value & 0xFFFFu)) & 0xFFFFu;
    word = (word & ~(0xFFFFu << 5)) | (imm16 << 5);
    put32(image, siteOffset, word);
}

// `movk`'s own 16-bit immediate (the high half) - unlike the low half,
// this needs the carry out of the low half's own addition (computed
// here by re-reading that already-patched sibling site, always
// exactly one instruction - 4 bytes - earlier, since
// Arm64Codegen.cpp always emits `movz` immediately followed by `movk`
// and GenericLinker::merge() patches relocations for one object in
// the order they were recorded, i.e. the low site's own relocation(s)
// always land before the high site's) - the exact carry bug once
// found and fixed in i8008's own Lo/Hi split (see that file's own
// linker factory for the postmortem).
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

// `0x44000000` is the same initial-stack-pointer value as
// Arm64Codegen.cpp's own (private) `kInitialSP` - comfortably above
// the loaded program and its data, same reasoning as that file's own
// comment. Split into its own low/high 16-bit halves exactly as
// Arm64Codegen.cpp's own entry sequence does for a plain (non-label)
// immediate.
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
    config.outputFormats = {"flat", "coff", "elf", "aout"};
    return std::make_unique<GenericLinker>(std::move(config));
}

}  // namespace pcc
