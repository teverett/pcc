#include "PowerPCLinkerFactory.h"

#include "codegen/powerpc/PowerPCAssembler.h"
#include "linker/GenericLinker.h"

namespace pcc {

namespace {

// Big-endian 32-bit load/store - every PowerPC instruction word here
// is always big-endian in memory (see PowerPCAssembler.cpp's own
// `emit32` comment), same byte order as m68k/mips.
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

// `b`/`bl` - opcode(6)=18 LI(24) AA(1)=0 LK(1), the LI field already
// zeroed by PowerPCAssembler.cpp's own `emitBranch()` (this relocation
// only ever fires for a target undefined in this object - see
// PowerPCAssembler.h's own `kPowerPCRelocBranch` comment on why a
// same-object branch needs no relocation at all). Real PowerPC B-form
// semantics: pure PC-relative from this instruction's own address, no
// "+8"/"+4" pipeline quirk the way classic ARM A32 has - see
// PowerPCAssembler.cpp's own `emitBranch()` comment, and
// Arm64LinkerFactory.cpp's own `patchA64Rel26()` for the identical
// (word-granularity, no-pipeline-quirk) reasoning on AArch64.
void patchPowerPCBranch24(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t siteFinalAddr) {
    int32_t rel = static_cast<int32_t>(value) - static_cast<int32_t>(siteFinalAddr);
    uint32_t word = get32BE(image, siteOffset);
    word = (word & ~0x03FFFFFCu) | (static_cast<uint32_t>(rel) & 0x03FFFFFCu);
    put32BE(image, siteOffset, word);
}

// `lis`'s own 16-bit immediate - the upper half of a `lis`/`ori`
// address-load pair (see PowerPCAssembler.cpp's own `lis`/`ori`
// handling in `encodeInstr()`). The two halves combine with a bitwise
// OR at the CPU, not an add, so there's no classic `@ha`/`@l` sign-
// extension correction to make here - but the *linker's own*
// correction (this object's own base, for `..`, or an external
// symbol's own full address) still has to be added to each 16-bit
// half separately, and that addition can still carry out of the low
// half into the high half. That carry is computed here by re-reading
// the paired Lo site (`siteOffset + 4` - PowerPCCodegen.cpp's own
// `loadAddress()` always emits `lis` immediately followed by `ori`)
// while it's still holding its own pre-patch value: GenericLinker::
// merge() patches relocations for one object in the order they were
// recorded, i.e. in code order, so the Hi site's own relocation
// (recorded when assembling the earlier `lis`) always lands - and is
// applied - before the Lo site's own (recorded when assembling the
// later `ori`), the exact same "read the sibling before it's patched"
// technique the i8008 migration's own `patchAbs16Hi`/`patchAbs16Lo`
// pair uses for its identical carry issue.
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

// `ori`'s own 16-bit immediate - the lower half of the pair above.
// Truncated mod-65536 addition is always carry-correct on its own;
// this is the sibling site patchPowerPCHi() above reads while it
// still holds this field's own pre-patch value, so this function must
// (and - see that function's own comment on the recorded-order
// guarantee - does) only ever patch strictly after the Hi site.
void patchPowerPCLo(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    uint32_t word = get32BE(image, siteOffset);
    uint32_t imm16 = word & 0xFFFFu;
    imm16 = (imm16 + (value & 0xFFFFu)) & 0xFFFFu;
    word = (word & ~0xFFFFu) | imm16;
    put32BE(image, siteOffset, word);
}

// `32`/`0` are `kInitialSP`'s (0x00200000, PowerPCCodegen.cpp's own
// private constant) upper/lower 16 bits, split exactly as that file's
// own entry sequence already does for a plain (non-label) immediate -
// see PowerPCCodegen.cpp's own `generate()`.
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
    config.outputFormats = {"flat", "coff", "elf", "aout"};
    return std::make_unique<GenericLinker>(std::move(config));
}

}  // namespace pcc
