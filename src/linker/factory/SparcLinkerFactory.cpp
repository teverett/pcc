#include "SparcLinkerFactory.h"

#include "codegen/sparc/SparcAssembler.h"
#include "linker/GenericLinker.h"

namespace pcc {

namespace {

// Big-endian 32-bit load/store - every SPARC instruction word here is
// always big-endian in memory (see SparcAssembler.cpp's own `emit32`
// comment), same byte order as m68k/mips/powerpc.
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

// `call` - format 1: op(2)=01 disp30(30), the disp30 field already
// zeroed by SparcAssembler.cpp's own `encodeCall()` (this relocation
// only ever fires for a target undefined in this object - see
// SparcAssembler.h's own `kSparcRelocCall` comment on why a
// same-object call needs no relocation at all). Real SPARC CALL
// semantics: pure PC-relative from this instruction's own address, no
// "+8"/"+4" pipeline quirk the way classic ARM A32 has - see
// SparcAssembler.cpp's own `encodeCall()` comment, and
// PowerPCLinkerFactory.cpp's own `patchPowerPCBranch24()` for the
// identical (word-granularity, no-pipeline-quirk) reasoning on
// PowerPC.
void patchSparcCall30(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t siteFinalAddr) {
    int32_t rel = static_cast<int32_t>(value) - static_cast<int32_t>(siteFinalAddr);
    uint32_t disp30 = (static_cast<uint32_t>(rel) >> 2) & 0x3FFFFFFFu;
    uint32_t word = get32BE(image, siteOffset);
    word = (word & 0xC0000000u) | disp30;
    put32BE(image, siteOffset, word);
}

// `sethi`'s own 22-bit immediate - the upper half of a `sethi`/`or`
// address-load pair (see SparcAssembler.cpp's own `sethi`/`or`
// handling in `encodeInstr()`). The two halves combine with a bitwise
// OR at the CPU, not an add, so there's no classic PowerPC-`@ha`/`@l`-
// style sign-extension correction to make here - but the *linker's*
// own correction (this object's own base, for `..`, or an external
// symbol's own full address) still has to be added to each field
// separately, and that addition can still carry out of the 10-bit Lo
// field into the 22-bit Hi field. That carry is computed here by
// re-reading the paired Lo site (`siteOffset + 4` -
// SparcCodegen.cpp's own `loadAddress()` always emits `sethi`
// immediately followed by `or`) while it's still holding its own
// pre-patch value: GenericLinker::merge() patches relocations for one
// object in the order they were recorded, i.e. in code order, so the
// Hi site's own relocation (recorded when assembling the earlier
// `sethi`) always lands - and is applied - before the Lo site's own
// (recorded when assembling the later `or`), the exact same
// "read the sibling before it's patched" technique
// PowerPCLinkerFactory.cpp's own `patchPowerPCHi`/`patchPowerPCLo`
// pair (ported from the i8008 migration's own `patchAbs16Hi`/
// `patchAbs16Lo`) uses for its identical carry issue.
void patchSparcHi(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    uint32_t loWord = get32BE(image, siteOffset + 4);
    uint32_t loField = loWord & 0x3FFu;
    uint32_t carry = (loField + (value & 0x3FFu)) > 0x3FFu ? 1u : 0u;

    uint32_t word = get32BE(image, siteOffset);
    uint32_t imm22 = word & 0x3FFFFFu;
    imm22 = (imm22 + ((value >> 10) & 0x3FFFFFu) + carry) & 0x3FFFFFu;
    word = (word & ~0x3FFFFFu) | imm22;
    put32BE(image, siteOffset, word);
}

// `or`'s own lower-10-bits field - the lower half of the pair above.
// Truncated mod-1024 addition is always carry-correct on its own;
// this is the sibling site patchSparcHi() above reads while it still
// holds this field's own pre-patch value, so this function must (and -
// see that function's own comment on the recorded-order guarantee -
// does) only ever patch strictly after the Hi site.
void patchSparcLo(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    uint32_t word = get32BE(image, siteOffset);
    uint32_t imm10 = word & 0x3FFu;
    imm10 = (imm10 + (value & 0x3FFu)) & 0x3FFu;
    word = (word & ~0x3FFu) | imm10;
    put32BE(image, siteOffset, word);
}

// `1024`/`0` are `kInitialSP`'s (0x00100000, SparcCodegen.cpp's own
// private constant) upper-22/lower-10 bits, split exactly as that
// file's own entry sequence already does for a plain (non-label)
// immediate - see SparcCodegen.cpp's own `generate()`.
constexpr const char *kEntryAsm =
    "org 0\n"
    "_start:\n"
    "    sethi 1024,sp\n"
    "    or sp,0,sp\n"
    "    call fn_main\n"
    "halt:\n"
    "    ba halt\n";

}  // namespace

std::unique_ptr<TargetLinker> makeSparcLinker() {
    GenericLinkerConfig config;
    config.baseAddress = kSparcBaseAddress;
    config.machineType = kSparcCoffMachine;
    config.entryObject = assembleSparcObject(kEntryAsm);
    config.relocPatchers[kSparcRelocHi] = patchSparcHi;
    config.relocPatchers[kSparcRelocLo] = patchSparcLo;
    config.relocPatchers[kSparcRelocCall] = patchSparcCall30;
    config.outputFormats = {"flat", "coff", "elf", "aout"};
    return std::make_unique<GenericLinker>(std::move(config));
}

}  // namespace pcc
