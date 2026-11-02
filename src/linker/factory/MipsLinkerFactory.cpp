#include "MipsLinkerFactory.h"

#include "codegen/mips/MipsAssembler.h"
#include "linker/GenericLinker.h"

namespace pcc {

namespace {

// Big-endian 32-bit instruction-word access - every MIPS instruction
// here is a fixed 4 bytes, high-byte-first (see MipsAssembler.cpp's
// own `emit32`).
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

// `ori`'s own 16-bit immediate (the low half of a full address) -
// additive (the field already holds whatever locally-resolved half
// the assembler baked in - see MipsAssembler.h's own header comment
// on why every absolute-address reference needs this, not just an
// external one). A plain truncated mod-65536 addition is always
// carry-correct on its own, regardless of patch order - see
// ../../codegen/i8008/I8008LinkerFactory.cpp's own identical reasoning
// for its own Lo/Hi split.
void patchMipsLo(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    uint32_t word = get32BE(image, siteOffset);
    uint32_t imm16 = ((word & 0xFFFFu) + (value & 0xFFFFu)) & 0xFFFFu;
    put32BE(image, siteOffset, (word & 0xFFFF0000u) | imm16);
}

// `lui`'s own 16-bit immediate (the high half) - unlike the low half,
// this needs the carry out of the low half's own addition. Computed
// here by reading the paired `ori` site's own 16-bit immediate,
// always exactly one instruction (4 bytes) later, since
// MipsCodegen.cpp's own `loadAddress()` always emits `lui` immediately
// followed by `ori` for the same address, and MipsAssembler.cpp's own
// `encodeHiLo()` records each one's relocation (if any) at the point
// it's encoded - `lui` first, `ori` second. Because
// GenericLinker::merge() patches one object's own relocations in a
// single left-to-right pass over that list, this Hi site is always
// patched before the paired Lo site below it, so the Lo field this
// reads is still its original, pre-patch value - exactly what the
// carry computation needs (see
// ../../codegen/i8008/I8008LinkerFactory.cpp's own identical Hi-
// before-Lo convention for the same reason).
void patchMipsHi(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    uint32_t loImm = get32BE(image, siteOffset + 4) & 0xFFFFu;
    uint32_t carry = (loImm + (value & 0xFFFFu)) > 0xFFFFu ? 1u : 0u;
    uint32_t word = get32BE(image, siteOffset);
    uint32_t imm16 = ((word & 0xFFFFu) + ((value >> 16) & 0xFFFFu) + carry) & 0xFFFFu;
    put32BE(image, siteOffset, (word & 0xFFFF0000u) | imm16);
}

// `j`/`jal`'s own 26-bit instr_index field (bits [25:0] of the
// instruction word - the opcode occupies bits [31:26], see
// MipsAssembler.cpp's own `emitJ`). Both this site's own pre-patch
// addend and `value` (the resolved symbol's own linked address, or
// this object's own linked base for the ".." pseudo-symbol) are
// always word-aligned - every label and every object's own base here
// is a multiple of 4 - so right-shifting each by 2 and then adding
// gives exactly the same result as adding them first and then
// shifting: no carry-from-the-dropped-low-bits subtlety the way the
// Hi/Lo split above has, so a plain additive patch on the 26-bit
// field is exact.
void patchMipsJump(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    uint32_t word = get32BE(image, siteOffset);
    uint32_t index = ((word & 0x03FFFFFFu) + (value >> 2)) & 0x03FFFFFFu;
    put32BE(image, siteOffset, (word & 0xFC000000u) | index);
}

// `32784`/`0` is `0x80100000`'s own high/low 16-bit halves - the same
// initial-stack-pointer value as MipsCodegen.cpp's own (private)
// `kInitialSP` - comfortably above the loaded program and its data,
// same reasoning as that file's own comment. `lui`/`ori` here take a
// bare number, not a label, so neither needs a relocation.
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
    config.outputFormats = {"flat", "coff", "elf", "aout"};
    return std::make_unique<GenericLinker>(std::move(config));
}

}  // namespace pcc
