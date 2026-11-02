#include "ArmMLinkerFactory.h"

#include "codegen/armm/ArmMAssembler.h"
#include "linker/GenericLinker.h"

namespace pcc {

namespace {

// Little-endian 16-bit halfword load/store - every Thumb/Thumb-2
// instruction halfword here is always little-endian in memory, same
// as every other field ArmMAssembler.cpp's own `emit16` writes.
uint16_t get16(const std::vector<uint8_t> &image, size_t idx) {
    return static_cast<uint16_t>(static_cast<uint8_t>(image[idx]) |
                                  (static_cast<uint16_t>(static_cast<uint8_t>(image[idx + 1])) << 8));
}
void put16(std::vector<uint8_t> &image, size_t idx, uint16_t v) {
    image[idx] = static_cast<uint8_t>(v & 0xFF);
    image[idx + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
}

// Pulls `MOVW`/`MOVT`'s own 16-bit immediate back out of its two
// already-encoded halfwords - the exact inverse of
// ArmMAssembler.cpp's own `encodeMovWT` (imm16 =
// imm4:i:imm3:imm8, scattered across both halfwords - see
// ArmMAssembler.h's own header comment on why this needs its own
// dedicated relocation kind instead of one of GenericLinker.h's
// shared ones).
uint32_t extractImm16(uint16_t h1, uint16_t h2) {
    uint32_t imm4 = h1 & 0xFu;
    uint32_t i = (h1 >> 10) & 1u;
    uint32_t imm3 = (h2 >> 12) & 0x7u;
    uint32_t imm8 = h2 & 0xFFu;
    return (imm4 << 12) | (i << 11) | (imm3 << 8) | imm8;
}
// The inverse: splits `imm16` back into its own four sub-fields and
// writes them into `h1`/`h2`'s own bit positions, leaving every other
// bit (the fixed opcode bits, and `h2`'s own `Rd` field) untouched.
void injectImm16(uint16_t &h1, uint16_t &h2, uint32_t imm16) {
    uint32_t imm4 = (imm16 >> 12) & 0xFu;
    uint32_t i = (imm16 >> 11) & 1u;
    uint32_t imm3 = (imm16 >> 8) & 0x7u;
    uint32_t imm8 = imm16 & 0xFFu;
    h1 = static_cast<uint16_t>((h1 & ~0x040Fu) | (i << 10) | imm4);
    h2 = static_cast<uint16_t>((h2 & ~0x70FFu) | (imm3 << 12) | imm8);
}

// `MOVW`'s own low 16 bits of a full address - additive (the field
// already holds whatever locally-resolved half the assembler baked
// in - see Arm64LinkerFactory.cpp's own `patchA64MovZLo` for the
// identical reasoning, just applied to this dialect's own
// non-contiguous split instead of A64's contiguous 16-bit field).
void patchArmMMovWLo(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    uint16_t h1 = get16(image, siteOffset);
    uint16_t h2 = get16(image, siteOffset + 2);
    uint32_t imm16 = (extractImm16(h1, h2) + (value & 0xFFFFu)) & 0xFFFFu;
    injectImm16(h1, h2, imm16);
    put16(image, siteOffset, h1);
    put16(image, siteOffset + 2, h2);
}

// `MOVT`'s own high 16 bits - needs the carry out of the low half's
// own addition, computed here by re-reading that already-patched
// sibling site, always exactly one whole `MOVW` instruction (4 bytes)
// earlier, since ArmMCodegen.cpp always emits `MOVW` immediately
// followed by `MOVT` (see its own `loadAddress()`) and
// GenericLinker::merge() patches relocations for one object in the
// order they were recorded, i.e. the low site's own relocation
// always lands before the high site's - the same carry-propagation
// scheme Arm64LinkerFactory.cpp's own `patchA64MovKHi` uses, just
// applied to this dialect's own non-contiguous split.
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

// `BL`/wide-`B`'s own 32-bit PC-relative branch-immediate field
// (also the wide `B.W` half of every conditional-branch expansion -
// see ArmMAssembler.cpp's own `encodeConditionalBranch()`) - fires
// only for a target undefined in this object (see
// ArmMAssembler.h's own header comment on why a same-object branch
// needs no relocation at all). Unlike the additive MOVW/MOVT patches
// above, this one's field only ever holds zero going in (the
// assembler always defers with a placeholder zero offset when it
// can't resolve the target locally - see `encodeWideBranch()`), so
// it's recomputed from scratch rather than added into: `value -
// (siteFinalAddr + 4)`, the same "+4" real-Thumb-2-PC convention
// ArmMAssembler.cpp's own encoder uses, halved (the offset is always
// even) and scrambled across both halfwords via the real
// S/I1/I2/J1/J2 encoding - the exact inverse of that same encoder.
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

// `1048576` (`0x100000`) is the same initial-stack-pointer value as
// ArmMCodegen.cpp's own (private) `kInitialSP` - comfortably above
// the loaded program and its data, same reasoning as that file's own
// comment. Split into its own low/high halves exactly as
// ArmMCodegen.cpp's own `loadImm32()` does for a plain (non-label)
// immediate bigger than 8 bits (`0` low, `16` high).
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
    config.outputFormats = {"flat", "coff", "elf", "aout"};
    return std::make_unique<GenericLinker>(std::move(config));
}

}  // namespace pcc
