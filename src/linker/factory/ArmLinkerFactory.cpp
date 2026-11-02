#include "ArmLinkerFactory.h"

#include "codegen/arm/ArmAssembler.h"
#include "linker/GenericLinker.h"

namespace pcc {

namespace {

// Little-endian 32-bit load/store - every A32 instruction word here
// is always little-endian in memory (see ArmAssembler.cpp's own
// `emit32` comment).
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

// `B`/`BL`'s own 24-bit offset field (bits [23:0]), word-granularity -
// the field itself is left zero by ArmAssembler.cpp's own
// encodeBranch() for a target undefined within its own object (see
// that function's own comment), so this patcher only ever needs to
// write the field, never add to it. The "+8" reproduces A32's own
// architectural "PC reads as this instruction's own address plus 8"
// convention (see ArmAssembler.cpp's own encodeBranch() for the
// identical arithmetic it uses for a same-object branch at assembly
// time) at link time instead, now that `value`/`siteFinalAddr` are
// both real linked addresses.
void patchArmRel24(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t siteFinalAddr) {
    int32_t rel = (static_cast<int32_t>(value) - static_cast<int32_t>(siteFinalAddr + 8)) / 4;
    uint32_t word = get32(image, siteOffset);
    word = (word & ~0x00FFFFFFu) | (static_cast<uint32_t>(rel) & 0x00FFFFFFu);
    put32(image, siteOffset, word);
}

// `MOVW`'s own imm4:imm12 split (bits [19:16] and [11:0]) - additive
// (the field already holds whatever locally-resolved half the
// assembler baked in - see Arm64LinkerFactory.cpp's own
// `patchA64MovZLo` for the identical "already the right half, just
// add the other" convention). Low half: a plain truncated mod-65536
// addition is always carry-correct on its own.
void patchArmMovWLo(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    uint32_t word = get32(image, siteOffset);
    uint32_t imm16 = (((word >> 16) & 0xFu) << 12) | (word & 0xFFFu);
    imm16 = (imm16 + (value & 0xFFFFu)) & 0xFFFFu;
    word = (word & ~(0xFu << 16) & ~0xFFFu) | (((imm16 >> 12) & 0xFu) << 16) | (imm16 & 0xFFFu);
    put32(image, siteOffset, word);
}

// `MOVT`'s own imm4:imm12 split (the high half) - needs the carry out
// of the low half's own addition, computed here by re-reading that
// already-patched sibling site, always exactly one instruction (4
// bytes) earlier, since ArmCodegen.cpp's own loadAddress() always
// emits `movw` immediately followed by `movt` for the same label, and
// GenericLinker::merge() patches relocations for one object in the
// order ArmAssembler.cpp recorded them, i.e. the low site's own
// relocation always lands before the high site's - same precedent as
// Arm64LinkerFactory.cpp's own `patchA64MovKHi`.
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

// `0x100000` is the same initial-stack-pointer value as
// ArmCodegen.cpp's own (private) `kInitialSP` - comfortably above the
// loaded program and its data, same reasoning as that file's own
// comment. Split into its own low/high 16-bit halves exactly as
// ArmCodegen.cpp's own entry sequence does for a plain (non-label)
// immediate.
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

}  // namespace pcc
